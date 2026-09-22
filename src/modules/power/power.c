#include "modules/power/power.h"

#include <string.h>
#include "board.h"
#include "board_config.h"
#include "modules/console/console.h"
#include "modules/system/system.h"
#include "tx_api.h"

#define POWER_THREAD_STACK_SIZE 1536U
#define POWER_THREAD_PRIORITY 11U
#define POWER_SAMPLE_PERIOD_MS (1000U / BOARD_CHARGER_SAMPLE_HZ)

static TX_THREAD power_thread;
static ULONG power_thread_stack[POWER_THREAD_STACK_SIZE / sizeof(ULONG)];
static TX_MUTEX power_lock;
static uint8_t power_ready;
static uint8_t shp_print_div;
static power_bq_snapshot snap;
static power_shp_snapshot shp_snap;
static const bq25756 charger = {.i2c_inst = 1U,
                                .address = BQ25756_I2C_ADDRESS,
                                .input_sense_mohm = BOARD_CHARGER_SENSE_MOHM,
                                .battery_sense_mohm = BOARD_CHARGER_SENSE_MOHM,
                                .timeout_cycles = 50U};
static const shp8808 shp = {.i2c_inst = 1U,
                            .address = SHP8808_I2C_ADDRESS,
                            .input_sense_mohm = BOARD_SHP_IBUS_SENSE_MOHM,
                            .battery_sense_mohm = BOARD_SHP_IBAT_SENSE_MOHM,
                            .timeout_cycles = 50U};

static uint32_t current_from_reg(uint16_t raw, uint16_t sense_mohm)
{
    uint32_t code = ((uint32_t)raw >> 2) & 0x3FFU;

    if (sense_mohm == 0U)
        sense_mohm = 5U;
    return (code * 250U) / sense_mohm;
}

static uint32_t voltage_20mv_from_reg(uint16_t raw)
{
    return (((uint32_t)raw >> 2) & 0x0FFFU) * 20U;
}

static uint32_t shp_ibat_ma(uint32_t code, uint32_t offset_ma)
{
    uint16_t sense = shp.battery_sense_mohm == 0U ? 5U : shp.battery_sense_mohm;

    return (code * 500U + offset_ma * 5U) / sense;
}

static uint32_t shp_ibus_ma(uint32_t code)
{
    uint16_t sense = shp.input_sense_mohm == 0U ? 2U : shp.input_sense_mohm;

    return (code * 750U + 4750U) / sense;
}

static uint32_t shp_otg_mv(uint16_t raw)
{
    uint16_t code = raw & 0x03FFU;

    if ((raw & 0x0400U) != 0U)
        return 21050U + (uint32_t)code * 50U;
    return 540U + (uint32_t)code * 20U;
}

static void log_event(uint8_t kind, uint8_t bits)
{
    power_fault_event *ev;

    if (bits == 0U)
        return;
    if (snap.log_count < POWER_FAULT_LOG_MAX) {
        ev = &snap.log[snap.log_count++];
    } else {
        (void)memmove(&snap.log[0], &snap.log[1], sizeof(snap.log[0]) * (POWER_FAULT_LOG_MAX - 1U));
        ev = &snap.log[POWER_FAULT_LOG_MAX - 1U];
    }
    ev->t_ms = system_uptime_ms();
    ev->kind = kind;
    ev->bits = bits;
}

static void shp_log_event(uint8_t kind, uint8_t bits)
{
    power_fault_event *ev;

    if (bits == 0U)
        return;
    if (shp_snap.log_count < POWER_FAULT_LOG_MAX) {
        ev = &shp_snap.log[shp_snap.log_count++];
    } else {
        (void)memmove(&shp_snap.log[0], &shp_snap.log[1], sizeof(shp_snap.log[0]) * (POWER_FAULT_LOG_MAX - 1U));
        ev = &shp_snap.log[POWER_FAULT_LOG_MAX - 1U];
    }
    ev->t_ms = system_uptime_ms();
    ev->kind = kind;
    ev->bits = bits;
}

static int refresh_locked(void)
{
    uint16_t u16;
    uint8_t u8;
    bq25756_flags flags;
    bq25756_status_snapshot raw;
    uint8_t rise;

    if (bq25756_read_and_clear_flags(&charger, &flags) == BQ25756_OK) {
        rise = (uint8_t)(flags.charger1 & (uint8_t)~snap.latch_flag1);
        snap.latch_flag1 |= flags.charger1;
        log_event(1U, rise);
        rise = (uint8_t)(flags.charger2 & (uint8_t)~snap.latch_flag2);
        snap.latch_flag2 |= flags.charger2;
        log_event(2U, rise);
        rise = (uint8_t)(flags.fault & (uint8_t)~snap.latch_fault_flag);
        snap.latch_fault_flag |= flags.fault;
        log_event(3U, rise);
    }
    if (bq25756_read_status(&charger, &raw) != BQ25756_OK || bq25756_adc_read(&charger, &snap.adc) != BQ25756_OK)
        return -1;

    rise = (uint8_t)(raw.fault & (uint8_t)~snap.latch_fault);
    snap.latch_fault |= raw.fault;
    log_event(0U, rise);
    snap.raw = raw;
    bq25756_decode_status(&raw, &snap.decoded);
    snap.uptime_ms = system_uptime_ms();

    if (bq25756_read8(&charger, BQ25756_REG_CHARGER_CONTROL, &u8) == BQ25756_OK) {
        snap.charge_en = (u8 & 0x01U) != 0U;
        snap.hiz = (u8 & 0x04U) != 0U;
        snap.vrechg = (uint8_t)((u8 >> 6) & 0x03U);
        snap.pins.ce_pin_enabled = (u8 & 0x10U) == 0U;
    }
    if (bq25756_read8(&charger, BQ25756_REG_POWER_CONTROL, &u8) == BQ25756_OK) {
        snap.reverse_en = (u8 & 0x01U) != 0U;
        snap.pfm = (u8 & 0x20U) != 0U;
    }
    if (bq25756_read8(&charger, BQ25756_REG_MPPT_CONTROL, &u8) == BQ25756_OK) {
        snap.mppt_en = (u8 & 0x01U) != 0U;
        snap.mppt.full_sweep = (bq25756_mppt_sweep_timer)((u8 >> 1) & 0x03U);
        snap.mppt.perturb = (bq25756_mppt_po_timer)((u8 >> 5) & 0x03U);
    }
    if (bq25756_read8(&charger, BQ25756_REG_TS_CHARGE_BEHAVIOR, &u8) == BQ25756_OK) {
        snap.ts_en = (u8 & 0x01U) != 0U;
        snap.jeita_en = (u8 & 0x02U) != 0U;
    }
    if (bq25756_read8(&charger, BQ25756_REG_TIMER_CONTROL, &u8) == BQ25756_OK) {
        snap.safety_en = (u8 & 0x08U) != 0U;
        snap.safety_timer = (uint8_t)((u8 >> 1) & 0x03U);
        snap.watchdog = (uint8_t)((u8 >> 4) & 0x03U);
        snap.topoff = (uint8_t)((u8 >> 6) & 0x03U);
    }
    if (bq25756_read8(&charger, BQ25756_REG_PRECHARGE_CONTROL, &u8) == BQ25756_OK) {
        snap.charge.enable_precharge = (u8 & 0x01U) != 0U;
        snap.vbat_lowv = (uint8_t)((u8 >> 1) & 0x03U);
        snap.charge.enable_termination = (u8 & 0x08U) != 0U;
    }
    if (bq25756_read8(&charger, BQ25756_REG_PIN_CONTROL, &u8) == BQ25756_OK) {
        snap.pins.ichg_pin_enabled = (u8 & 0x80U) != 0U;
        snap.pins.ilim_hiz_pin_enabled = (u8 & 0x40U) != 0U;
        snap.pins.pg_pin_enabled = (u8 & 0x20U) == 0U;
        snap.pins.stat_pins_enabled = (u8 & 0x10U) == 0U;
    }
    if (bq25756_read16(&charger, BQ25756_REG_CHARGE_VOLTAGE, &u16) == BQ25756_OK)
        snap.charge.fb_voltage_mv = (uint16_t)(1504U + ((u16 & 0x1FU) * 2U));
    if (bq25756_read16(&charger, BQ25756_REG_CHARGE_CURRENT, &u16) == BQ25756_OK)
        snap.charge.charge_current_ma = current_from_reg(u16, charger.battery_sense_mohm);
    if (bq25756_read16(&charger, BQ25756_REG_INPUT_CURRENT, &u16) == BQ25756_OK)
        snap.charge.input_current_ma = current_from_reg(u16, charger.input_sense_mohm);
    if (bq25756_read16(&charger, BQ25756_REG_INPUT_VOLTAGE, &u16) == BQ25756_OK)
        snap.charge.input_voltage_mv = voltage_20mv_from_reg(u16);
    if (bq25756_read16(&charger, BQ25756_REG_PRECHARGE_CURRENT, &u16) == BQ25756_OK)
        snap.charge.precharge_current_ma = current_from_reg(u16, charger.battery_sense_mohm);
    if (bq25756_read16(&charger, BQ25756_REG_TERMINATION_CURRENT, &u16) == BQ25756_OK)
        snap.charge.termination_current_ma = current_from_reg(u16, charger.battery_sense_mohm);
    if (bq25756_read16(&charger, BQ25756_REG_REVERSE_VOLTAGE, &u16) == BQ25756_OK)
        snap.reverse.vac_mv = voltage_20mv_from_reg(u16);
    if (bq25756_read16(&charger, BQ25756_REG_REVERSE_CURRENT, &u16) == BQ25756_OK)
        snap.reverse.iac_ma = current_from_reg(u16, charger.input_sense_mohm);
    if (bq25756_read8(&charger, BQ25756_REG_REVERSE_BAT_CURRENT, &u8) == BQ25756_OK)
        snap.reverse.ibat_rev = (bq25756_ibat_rev)((u8 >> 6) & 0x03U);
    if (bq25756_read8(&charger, BQ25756_REG_REVERSE_UNDERVOLTAGE, &u8) == BQ25756_OK)
        snap.reverse.uvp_fixed_3v3 = (u8 & 0x20U) != 0U;
    if (bq25756_read8(&charger, BQ25756_REG_GATE_DRIVE_STRENGTH, &u8) == BQ25756_OK) {
        snap.gate.buck_ls = (bq25756_drv_strength)(u8 & 0x03U);
        snap.gate.boost_ls = (bq25756_drv_strength)((u8 >> 2) & 0x03U);
        snap.gate.buck_hs = (bq25756_drv_strength)((u8 >> 4) & 0x03U);
        snap.gate.boost_hs = (bq25756_drv_strength)((u8 >> 6) & 0x03U);
    }
    if (bq25756_read8(&charger, BQ25756_REG_GATE_DRIVE_DEAD_TIME, &u8) == BQ25756_OK) {
        snap.gate.buck_dead_time = (bq25756_dead_time)(u8 & 0x03U);
        snap.gate.boost_dead_time = (bq25756_dead_time)((u8 >> 2) & 0x03U);
    }
    if (snap.watchdog != (uint8_t)BQ25756_WATCHDOG_DISABLED)
        (void)bq25756_kick_watchdog(&charger);
    return 0;
}

static int shp_refresh_locked(void)
{
    uint16_t u16;
    uint8_t u8;
    shp8808_flags flags;
    shp8808_status_snapshot raw;
    uint8_t rise;

    if (shp8808_read_and_clear_flags(&shp, &flags) == SHP8808_OK) {
        rise = (uint8_t)(flags.status0 & 0x0EU & (uint8_t)~shp_snap.latch0);
        shp_snap.latch0 |= (uint8_t)(flags.status0 & 0x0EU);
        shp_log_event(0U, rise);
        rise = (uint8_t)(flags.status1 & 0x1FU & (uint8_t)~shp_snap.latch1);
        shp_snap.latch1 |= (uint8_t)(flags.status1 & 0x1FU);
        shp_log_event(1U, rise);
        rise = (uint8_t)(flags.status2 & 0xF0U & (uint8_t)~shp_snap.latch2);
        shp_snap.latch2 |= (uint8_t)(flags.status2 & 0xF0U);
        shp_log_event(2U, rise);
    }
    if (shp8808_read8(&shp, SHP8808_REG_ADC_CONTROL, &u8) == SHP8808_OK)
        shp_snap.adc_control = u8;
    if (shp8808_read_status(&shp, &raw) != SHP8808_OK || shp8808_adc_read(&shp, &shp_snap.adc) != SHP8808_OK)
        return -1;

    shp_snap.raw = raw;
    shp8808_decode_status(&raw, &shp_snap.decoded);
    shp_snap.uptime_ms = system_uptime_ms();
    if (shp8808_read8(&shp, SHP8808_REG_CHARGE_OPTION0, &u8) == SHP8808_OK) {
        shp_snap.option0 = u8;
        shp_snap.vbat_lowv = (uint8_t)((u8 >> 6) & 0x03U);
        shp_snap.vrechg = (uint8_t)((u8 >> 4) & 0x03U);
        shp_snap.vfloat = (uint8_t)((u8 >> 2) & 0x03U);
        shp_snap.hiz = (u8 & 0x02U) != 0U;
        shp_snap.charge_en = (u8 & 0x02U) == 0U;
    }
    if (shp8808_read8(&shp, SHP8808_REG_CHARGE_OPTION1, &u8) == SHP8808_OK) {
        shp_snap.pins.ibus_pin_enabled = (u8 & 0x80U) != 0U;
        shp_snap.pins.ibat_pin_enabled = (u8 & 0x40U) != 0U;
        shp_snap.charge.enable_termination = (u8 & 0x10U) != 0U;
        shp_snap.charge.enable_precharge = (u8 & 0x08U) != 0U;
        shp_snap.charge.enable_float = (u8 & 0x04U) != 0U;
    }
    if (shp8808_read8(&shp, SHP8808_REG_CHARGE_OPTION3, &u8) == SHP8808_OK) {
        shp_snap.precharge_timer_en = (u8 & 0x10U) != 0U;
        shp_snap.safety_en = (u8 & 0x08U) != 0U;
        shp_snap.precharge_timer = (uint8_t)((u8 >> 2) & 0x01U);
        shp_snap.safety_timer = (uint8_t)(u8 & 0x03U);
    }
    if (shp8808_read8(&shp, SHP8808_REG_CHARGE_OPTION4, &u8) == SHP8808_OK) {
        shp_snap.reverse_en = (u8 & 0x80U) != 0U;
        shp_snap.pins.otg_fb_pin_enabled = (u8 & 0x40U) != 0U;
        shp_snap.reverse.fb_pin_enabled = shp_snap.pins.otg_fb_pin_enabled;
        shp_snap.pfm = (u8 & 0x03U) != 0U;
    }
    if (shp8808_read8(&shp, SHP8808_REG_MPPT_CONTROL0, &u8) == SHP8808_OK) {
        shp_snap.mppt_en = (u8 & 0x80U) != 0U;
        shp_snap.mppt.full_sweep = (shp8808_mppt_sweep_timer)((u8 >> 4) & 0x03U);
        shp_snap.mppt.perturb = (shp8808_mppt_po_timer)((u8 >> 2) & 0x03U);
        shp_snap.mppt.step = (shp8808_mppt_step)(u8 & 0x03U);
    }
    if (shp8808_read8(&shp, SHP8808_REG_JEITA_CONTROL0, &u8) == SHP8808_OK)
        shp_snap.jeita_en = (u8 & 0x01U) != 0U;
    if (shp8808_read8(&shp, SHP8808_REG_JEITA_CONTROL1, &u8) == SHP8808_OK)
        shp_snap.ts_en = (u8 & 0x01U) == 0U;
    if (shp8808_read8(&shp, SHP8808_REG_CHARGE_CURRENT, &u8) == SHP8808_OK)
        shp_snap.charge.charge_current_ma = shp_ibat_ma(u8, 1000U);
    if (shp8808_read8(&shp, SHP8808_REG_PRECHARGE_CURRENT, &u8) == SHP8808_OK)
        shp_snap.charge.precharge_current_ma = shp_ibat_ma(u8 & 0x7FU, 400U);
    if (shp8808_read8(&shp, SHP8808_REG_TERMINATION_CURRENT, &u8) == SHP8808_OK)
        shp_snap.charge.termination_current_ma = shp_ibat_ma(u8 & 0x3FU, 400U);
    if (shp8808_read8(&shp, SHP8808_REG_CHARGE_VOLTAGE, &u8) == SHP8808_OK)
        shp_snap.charge.fb_voltage_mv = (uint16_t)(1504U + ((u8 & 0x1FU) * 2U));
    if (shp8808_read8(&shp, SHP8808_REG_IINDPM_CURRENT, &u8) == SHP8808_OK)
        shp_snap.charge.input_current_ma = shp_ibus_ma(u8 & 0x7FU);
    if (shp8808_read16(&shp, SHP8808_REG_VINDPM_VOLTAGE, &u16) == SHP8808_OK)
        shp_snap.charge.vindpm_ref_mv = (uint16_t)((u16 & 0x07FFU) + 100U);
    if (shp8808_read16(&shp, SHP8808_REG_OTG_VOLTAGE, &u16) == SHP8808_OK)
        shp_snap.reverse.vbus_mv = shp_otg_mv(u16);
    if (shp8808_read8(&shp, SHP8808_REG_OTG_CURRENT, &u8) == SHP8808_OK)
        shp_snap.reverse.ibus_ma = shp_ibus_ma(u8 & 0x7FU);
    return 0;
}

static int bq_chip_init(uint8_t *part_info)
{
    uint8_t part = 0U;
    bq25756_irq_mask irq_mask;

    if (part_info != 0)
        *part_info = 0U;
    if (bq25756_probe(&charger, &part) != BQ25756_OK)
        return -1;
    if (part_info != 0)
        *part_info = part;

    bq25756_irq_mask_debug(&irq_mask);
    if (bq25756_configure_software_host(&charger) != BQ25756_OK ||
        bq25756_set_irq_mask(&charger, &irq_mask) != BQ25756_OK ||
        bq25756_adc_configure(&charger, BQ25756_ADC_MONITOR, true) != BQ25756_OK ||
        bq25756_adc_start(&charger) != BQ25756_OK) {
        (void)console_puts("bq: host/adc setup failed");
        return -1;
    }

    (void)console_print("bq: BQ25756 ready, part 0x%x, charge off\r\n", part);
    return 0;
}

static int shp_chip_init(uint8_t *option0)
{
    uint8_t value = 0U;

    if (option0 != 0)
        *option0 = 0U;
    if (shp8808_probe(&shp, &value) != SHP8808_OK)
        return -1;
    if (option0 != 0)
        *option0 = value;
    if (shp8808_configure_software_host(&shp) != SHP8808_OK ||
        shp8808_adc_configure(&shp, SHP8808_ADC_ALL, true) != SHP8808_OK) {
        (void)console_puts("shp: host/adc setup failed");
        return -1;
    }
    (void)console_print("shp: SHP8808 ready, option0 0x%x, I2C1 PB8/PB9 100 kHz, charge off\r\n", value);
    return 0;
}

static const char *json_field(const char *json, uint32_t len, const char *key)
{
    uint32_t klen = 0U;
    uint32_t i;

    while (key[klen] != '\0')
        ++klen;
    if (json == 0 || len < klen + 3U)
        return 0;
    for (i = 0U; i + klen + 2U < len; ++i) {
        if (json[i] == '"' && memcmp(json + i + 1U, key, klen) == 0 && json[i + 1U + klen] == '"') {
            const char *p = json + i + klen + 2U;
            const char *end = json + len;
            while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
                ++p;
            if (p < end && *p == ':') {
                ++p;
                while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
                    ++p;
                return p < end ? p : 0;
            }
        }
    }
    return 0;
}

static int json_str(const char *json, uint32_t len, const char *key, char *out, uint32_t out_len)
{
    const char *p = json_field(json, len, key);
    uint32_t n = 0U;

    if (p == 0 || *p != '"' || out == 0 || out_len == 0U)
        return -1;
    ++p;
    while (p[n] != '\0' && p[n] != '"' && n + 1U < out_len)
        ++n;
    memcpy(out, p, n);
    out[n] = '\0';
    return 0;
}

static int json_u32(const char *json, uint32_t len, const char *key, uint32_t *out)
{
    const char *p = json_field(json, len, key);
    uint32_t v = 0U;

    if (p == 0 || out == 0)
        return -1;
    if (*p < '0' || *p > '9')
        return -1;
    while (*p >= '0' && *p <= '9')
        v = v * 10U + (uint32_t)(*p++ - '0');
    *out = v;
    return 0;
}

static int json_bool(const char *json, uint32_t len, const char *key, bool *out)
{
    const char *p = json_field(json, len, key);

    if (p == 0 || out == 0)
        return -1;
    if (p[0] == 't') {
        *out = true;
        return 0;
    }
    if (p[0] == 'f') {
        *out = false;
        return 0;
    }
    if (p[0] == '1') {
        *out = true;
        return 0;
    }
    if (p[0] == '0') {
        *out = false;
        return 0;
    }
    return -1;
}

static int json_on(const char *json, uint32_t len, bool *out)
{
    return json_bool(json, len, "on", out);
}

int power_bq_copy_snapshot(power_bq_snapshot *out)
{
    if (out == 0)
        return -1;
    if (power_ready == 0U || tx_mutex_get(&power_lock, 200) != TX_SUCCESS) {
        (void)memset(out, 0, sizeof(*out));
        return -1;
    }
    *out = snap;
    (void)tx_mutex_put(&power_lock);
    return snap.present ? 0 : -1;
}

int power_bq_command(const char *json, uint32_t json_len)
{
    char cmd[24];
    bool on = false;
    uint32_t u = 0U;
    int rc = -1;

    if (json == 0 || json_str(json, json_len, "cmd", cmd, sizeof(cmd)) != 0)
        return -1;
    if (power_ready == 0U || tx_mutex_get(&power_lock, 500) != TX_SUCCESS)
        return -1;
    if (!snap.present)
        goto done;

    if (strcmp(cmd, "charge") == 0) {
        if (json_on(json, json_len, &on) == 0)
            rc = bq25756_set_charge_enabled(&charger, on) == BQ25756_OK ? 0 : -1;
    } else if (strcmp(cmd, "hiz") == 0) {
        if (json_on(json, json_len, &on) == 0)
            rc = bq25756_set_hiz(&charger, on) == BQ25756_OK ? 0 : -1;
    } else if (strcmp(cmd, "reverse") == 0) {
        if (json_on(json, json_len, &on) == 0)
            rc = bq25756_set_reverse_enabled(&charger, on) == BQ25756_OK ? 0 : -1;
    } else if (strcmp(cmd, "mppt") == 0) {
        if (json_on(json, json_len, &on) == 0)
            rc = bq25756_set_mppt_enabled(&charger, on) == BQ25756_OK ? 0 : -1;
    } else if (strcmp(cmd, "mppt_sweep") == 0) {
        rc = bq25756_force_mppt_sweep(&charger) == BQ25756_OK ? 0 : -1;
    } else if (strcmp(cmd, "pfm") == 0) {
        if (json_on(json, json_len, &on) == 0)
            rc = bq25756_set_pfm(&charger, on) == BQ25756_OK ? 0 : -1;
    } else if (strcmp(cmd, "ts") == 0) {
        if (json_on(json, json_len, &on) == 0)
            rc = bq25756_set_ts_enabled(&charger, on) == BQ25756_OK ? 0 : -1;
    } else if (strcmp(cmd, "jeita") == 0) {
        if (json_on(json, json_len, &on) == 0)
            rc = bq25756_set_jeita_enabled(&charger, on) == BQ25756_OK ? 0 : -1;
    } else if (strcmp(cmd, "kick") == 0) {
        rc = bq25756_kick_watchdog(&charger) == BQ25756_OK ? 0 : -1;
    } else if (strcmp(cmd, "reset") == 0) {
        rc = bq25756_reset_registers(&charger) == BQ25756_OK ? 0 : -1;
        if (rc == 0)
            rc = bq_chip_init(&snap.part);
    } else if (strcmp(cmd, "clear_latch") == 0) {
        snap.latch_fault = 0U;
        snap.latch_flag1 = 0U;
        snap.latch_flag2 = 0U;
        snap.latch_fault_flag = 0U;
        snap.log_count = 0U;
        rc = 0;
    } else if (strcmp(cmd, "watchdog") == 0) {
        if (json_u32(json, json_len, "period", &u) == 0)
            rc = bq25756_set_watchdog(&charger, (bq25756_watchdog_period)u) == BQ25756_OK ? 0 : -1;
    } else if (strcmp(cmd, "vbat_lowv") == 0) {
        if (json_u32(json, json_len, "v", &u) == 0)
            rc = bq25756_set_vbat_lowv(&charger, (bq25756_vbat_lowv)u) == BQ25756_OK ? 0 : -1;
    } else if (strcmp(cmd, "vrechg") == 0) {
        if (json_u32(json, json_len, "v", &u) == 0)
            rc = bq25756_set_recharge_threshold(&charger, (bq25756_vrechg)u) == BQ25756_OK ? 0 : -1;
    } else if (strcmp(cmd, "safety") == 0) {
        bool en = true;
        if (json_u32(json, json_len, "timer", &u) == 0) {
            (void)json_bool(json, json_len, "on", &en);
            rc = bq25756_set_safety_timer(&charger, (bq25756_safety_timer)u, en) == BQ25756_OK ? 0 : -1;
        }
    } else if (strcmp(cmd, "topoff") == 0) {
        if (json_u32(json, json_len, "timer", &u) == 0)
            rc = bq25756_set_topoff_timer(&charger, (bq25756_topoff_timer)u) == BQ25756_OK ? 0 : -1;
    } else if (strcmp(cmd, "charge_cfg") == 0) {
        bq25756_charge_config cfg = snap.charge;
        if (json_u32(json, json_len, "fb_mv", &u) == 0)
            cfg.fb_voltage_mv = (uint16_t)u;
        if (json_u32(json, json_len, "ichg_ma", &u) == 0)
            cfg.charge_current_ma = u;
        if (json_u32(json, json_len, "iac_ma", &u) == 0)
            cfg.input_current_ma = u;
        if (json_u32(json, json_len, "vac_mv", &u) == 0)
            cfg.input_voltage_mv = u;
        if (json_u32(json, json_len, "ipre_ma", &u) == 0)
            cfg.precharge_current_ma = u;
        if (json_u32(json, json_len, "iterm_ma", &u) == 0)
            cfg.termination_current_ma = u;
        (void)json_bool(json, json_len, "en_pre", &cfg.enable_precharge);
        (void)json_bool(json, json_len, "en_term", &cfg.enable_termination);
        rc = bq25756_configure_charging(&charger, &cfg) == BQ25756_OK ? 0 : -1;
    } else if (strcmp(cmd, "reverse_cfg") == 0) {
        bq25756_reverse_config cfg = snap.reverse;
        if (json_u32(json, json_len, "vac_mv", &u) == 0)
            cfg.vac_mv = u;
        if (json_u32(json, json_len, "iac_ma", &u) == 0)
            cfg.iac_ma = u;
        if (json_u32(json, json_len, "ibat_rev", &u) == 0)
            cfg.ibat_rev = (bq25756_ibat_rev)u;
        (void)json_bool(json, json_len, "uvp_3v3", &cfg.uvp_fixed_3v3);
        rc = bq25756_configure_reverse(&charger, &cfg) == BQ25756_OK ? 0 : -1;
    } else if (strcmp(cmd, "mppt_cfg") == 0) {
        bq25756_mppt_config cfg = snap.mppt;
        if (json_u32(json, json_len, "perturb", &u) == 0)
            cfg.perturb = (bq25756_mppt_po_timer)u;
        if (json_u32(json, json_len, "sweep", &u) == 0)
            cfg.full_sweep = (bq25756_mppt_sweep_timer)u;
        rc = bq25756_configure_mppt(&charger, &cfg) == BQ25756_OK ? 0 : -1;
    } else if (strcmp(cmd, "gate") == 0) {
        bq25756_gate_drive_config cfg = snap.gate;
        if (json_u32(json, json_len, "buck_hs", &u) == 0)
            cfg.buck_hs = (bq25756_drv_strength)u;
        if (json_u32(json, json_len, "buck_ls", &u) == 0)
            cfg.buck_ls = (bq25756_drv_strength)u;
        if (json_u32(json, json_len, "boost_hs", &u) == 0)
            cfg.boost_hs = (bq25756_drv_strength)u;
        if (json_u32(json, json_len, "boost_ls", &u) == 0)
            cfg.boost_ls = (bq25756_drv_strength)u;
        if (json_u32(json, json_len, "buck_dt", &u) == 0)
            cfg.buck_dead_time = (bq25756_dead_time)u;
        if (json_u32(json, json_len, "boost_dt", &u) == 0)
            cfg.boost_dead_time = (bq25756_dead_time)u;
        rc = bq25756_configure_gate_drive(&charger, &cfg) == BQ25756_OK ? 0 : -1;
    } else if (strcmp(cmd, "pins") == 0) {
        bq25756_pin_config cfg = snap.pins;
        (void)json_bool(json, json_len, "ichg", &cfg.ichg_pin_enabled);
        (void)json_bool(json, json_len, "ilim", &cfg.ilim_hiz_pin_enabled);
        (void)json_bool(json, json_len, "ce", &cfg.ce_pin_enabled);
        (void)json_bool(json, json_len, "stat", &cfg.stat_pins_enabled);
        (void)json_bool(json, json_len, "pg", &cfg.pg_pin_enabled);
        rc = bq25756_configure_pins(&charger, &cfg) == BQ25756_OK ? 0 : -1;
    }

    if (rc == 0)
        (void)refresh_locked();

done:
    (void)tx_mutex_put(&power_lock);
    return rc;
}

int power_shp_copy_snapshot(power_shp_snapshot *out)
{
    if (out == 0)
        return -1;
    if (power_ready == 0U || tx_mutex_get(&power_lock, 200) != TX_SUCCESS) {
        (void)memset(out, 0, sizeof(*out));
        return -1;
    }
    *out = shp_snap;
    (void)tx_mutex_put(&power_lock);
    return shp_snap.present ? 0 : -1;
}

int power_shp_command(const char *json, uint32_t json_len)
{
    char cmd[24];
    bool on = false;
    uint32_t u = 0U;
    int rc = -1;

    if (json == 0 || json_str(json, json_len, "cmd", cmd, sizeof(cmd)) != 0)
        return -1;
    if (power_ready == 0U || tx_mutex_get(&power_lock, 500) != TX_SUCCESS)
        return -1;
    if (!shp_snap.present)
        goto done;

    if (strcmp(cmd, "charge") == 0) {
        if (json_on(json, json_len, &on) == 0)
            rc = shp8808_set_charge_enabled(&shp, on) == SHP8808_OK ? 0 : -1;
    } else if (strcmp(cmd, "hiz") == 0) {
        if (json_on(json, json_len, &on) == 0)
            rc = shp8808_set_hiz(&shp, on) == SHP8808_OK ? 0 : -1;
    } else if (strcmp(cmd, "reverse") == 0) {
        if (json_on(json, json_len, &on) == 0)
            rc = shp8808_set_reverse_enabled(&shp, on) == SHP8808_OK ? 0 : -1;
    } else if (strcmp(cmd, "mppt") == 0) {
        if (json_on(json, json_len, &on) == 0)
            rc = shp8808_set_mppt_enabled(&shp, on) == SHP8808_OK ? 0 : -1;
    } else if (strcmp(cmd, "pfm") == 0) {
        if (json_on(json, json_len, &on) == 0)
            rc = shp8808_set_pfm(&shp, on) == SHP8808_OK ? 0 : -1;
    } else if (strcmp(cmd, "ts") == 0) {
        if (json_on(json, json_len, &on) == 0)
            rc = shp8808_set_ts_enabled(&shp, on) == SHP8808_OK ? 0 : -1;
    } else if (strcmp(cmd, "jeita") == 0) {
        if (json_on(json, json_len, &on) == 0)
            rc = shp8808_set_jeita_enabled(&shp, on) == SHP8808_OK ? 0 : -1;
    } else if (strcmp(cmd, "reset") == 0) {
        rc = shp8808_reset_registers(&shp) == SHP8808_OK ? 0 : -1;
        if (rc == 0)
            rc = shp_chip_init(&shp_snap.option0);
    } else if (strcmp(cmd, "clear_latch") == 0) {
        shp_snap.latch0 = 0U;
        shp_snap.latch1 = 0U;
        shp_snap.latch2 = 0U;
        shp_snap.log_count = 0U;
        rc = 0;
    } else if (strcmp(cmd, "vbat_lowv") == 0) {
        if (json_u32(json, json_len, "v", &u) == 0)
            rc = shp8808_set_vbat_lowv(&shp, (shp8808_vbat_lowv)u) == SHP8808_OK ? 0 : -1;
    } else if (strcmp(cmd, "vrechg") == 0) {
        if (json_u32(json, json_len, "v", &u) == 0)
            rc = shp8808_set_recharge_threshold(&shp, (shp8808_vrechg)u) == SHP8808_OK ? 0 : -1;
    } else if (strcmp(cmd, "vfloat") == 0) {
        if (json_u32(json, json_len, "v", &u) == 0)
            rc = shp8808_set_float_threshold(&shp, (shp8808_vfloat)u) == SHP8808_OK ? 0 : -1;
    } else if (strcmp(cmd, "safety") == 0) {
        bool en = true;
        if (json_u32(json, json_len, "timer", &u) == 0) {
            (void)json_bool(json, json_len, "on", &en);
            rc = shp8808_set_safety_timer(&shp, (shp8808_safety_timer)u, en) == SHP8808_OK ? 0 : -1;
        }
    } else if (strcmp(cmd, "precharge_tmr") == 0) {
        bool en = true;
        if (json_u32(json, json_len, "timer", &u) == 0) {
            (void)json_bool(json, json_len, "on", &en);
            rc = shp8808_set_precharge_timer(&shp, (shp8808_precharge_timer)u, en) == SHP8808_OK ? 0 : -1;
        }
    } else if (strcmp(cmd, "charge_cfg") == 0) {
        shp8808_charge_config cfg = shp_snap.charge;
        if (json_u32(json, json_len, "fb_mv", &u) == 0)
            cfg.fb_voltage_mv = (uint16_t)u;
        if (json_u32(json, json_len, "ichg_ma", &u) == 0)
            cfg.charge_current_ma = u;
        if (json_u32(json, json_len, "iac_ma", &u) == 0)
            cfg.input_current_ma = u;
        if (json_u32(json, json_len, "vindpm_mv", &u) == 0)
            cfg.vindpm_ref_mv = (uint16_t)u;
        if (json_u32(json, json_len, "ipre_ma", &u) == 0)
            cfg.precharge_current_ma = u;
        if (json_u32(json, json_len, "iterm_ma", &u) == 0)
            cfg.termination_current_ma = u;
        (void)json_bool(json, json_len, "en_pre", &cfg.enable_precharge);
        (void)json_bool(json, json_len, "en_term", &cfg.enable_termination);
        (void)json_bool(json, json_len, "en_float", &cfg.enable_float);
        rc = shp8808_configure_charging(&shp, &cfg) == SHP8808_OK ? 0 : -1;
    } else if (strcmp(cmd, "reverse_cfg") == 0) {
        shp8808_reverse_config cfg = shp_snap.reverse;
        if (json_u32(json, json_len, "vbus_mv", &u) == 0)
            cfg.vbus_mv = u;
        if (json_u32(json, json_len, "ibus_ma", &u) == 0)
            cfg.ibus_ma = u;
        (void)json_bool(json, json_len, "fb_pin", &cfg.fb_pin_enabled);
        rc = shp8808_configure_reverse(&shp, &cfg) == SHP8808_OK ? 0 : -1;
    } else if (strcmp(cmd, "mppt_cfg") == 0) {
        shp8808_mppt_config cfg = shp_snap.mppt;
        if (json_u32(json, json_len, "perturb", &u) == 0)
            cfg.perturb = (shp8808_mppt_po_timer)u;
        if (json_u32(json, json_len, "sweep", &u) == 0)
            cfg.full_sweep = (shp8808_mppt_sweep_timer)u;
        if (json_u32(json, json_len, "step", &u) == 0)
            cfg.step = (shp8808_mppt_step)u;
        rc = shp8808_configure_mppt(&shp, &cfg) == SHP8808_OK ? 0 : -1;
    } else if (strcmp(cmd, "pins") == 0) {
        shp8808_pin_config cfg = shp_snap.pins;
        (void)json_bool(json, json_len, "ibus", &cfg.ibus_pin_enabled);
        (void)json_bool(json, json_len, "ibat", &cfg.ibat_pin_enabled);
        (void)json_bool(json, json_len, "otg_fb", &cfg.otg_fb_pin_enabled);
        rc = shp8808_configure_pins(&shp, &cfg) == SHP8808_OK ? 0 : -1;
    }

    if (rc == 0)
        (void)shp_refresh_locked();

done:
    (void)tx_mutex_put(&power_lock);
    return rc;
}

static void power_thread_entry(ULONG arg)
{
    uint8_t option0 = 0U;
    uint8_t part_info = 0U;
    uint8_t waiting = 0U;

    (void)arg;
    if (board_i2c_open() != 0) {
        (void)console_puts("pwr: i2c open failed");
        return;
    }
    power_ready = 1U;

    while (!shp_snap.present && !snap.present) {
        if (shp_chip_init(&option0) == 0) {
            (void)tx_mutex_get(&power_lock, TX_WAIT_FOREVER);
            shp_snap.present = true;
            shp_snap.option0 = option0;
            (void)shp_refresh_locked();
            (void)tx_mutex_put(&power_lock);
        }
        if (bq_chip_init(&part_info) == 0) {
            (void)tx_mutex_get(&power_lock, TX_WAIT_FOREVER);
            snap.present = true;
            snap.part = part_info;
            (void)refresh_locked();
            (void)tx_mutex_put(&power_lock);
        }
        if (shp_snap.present || snap.present)
            break;
        if (waiting == 0U) {
            waiting = 1U;
            (void)console_puts("pwr: waiting SHP8808 0x6C or BQ25756 0x6B");
        }
        tx_thread_sleep(1000U);
    }

    for (;;) {
        (void)tx_mutex_get(&power_lock, TX_WAIT_FOREVER);
        if (shp_snap.present && shp_refresh_locked() != 0)
            (void)console_puts("shp: read failed");
        if (snap.present && refresh_locked() != 0)
            (void)console_puts("bq: read failed");
        if (++shp_print_div >= BOARD_CHARGER_SAMPLE_HZ) {
            shp_print_div = 0U;
            if (shp_snap.present)
                (void)console_print("shp: VBUS %lu mV VBAT %lu mV IBUS %d mA IBAT %d mA %s chg %u adc 0x%x\r\n",
                                    (unsigned long)shp_snap.adc.input_voltage_mv,
                                    (unsigned long)shp_snap.adc.battery_voltage_mv, (int)shp_snap.adc.input_current_ma,
                                    (int)shp_snap.adc.battery_current_ma,
                                    shp8808_charge_phase_name(shp_snap.decoded.charge),
                                    shp_snap.charge_en ? 1U : 0U, (unsigned)shp_snap.adc_control);
            else if (snap.present)
                (void)console_print("bq: VAC %lu mV VBAT %lu mV IAC %d mA IBAT %d mA %s chg %u\r\n",
                                    (unsigned long)snap.adc.input_voltage_mv,
                                    (unsigned long)snap.adc.battery_voltage_mv,
                                    (int)(snap.adc.input_current_ma_x10 / 10), (int)snap.adc.battery_current_ma,
                                    bq25756_charge_phase_name(snap.decoded.charge), snap.charge_en ? 1U : 0U);
        }
        (void)tx_mutex_put(&power_lock);
        tx_thread_sleep((ULONG)POWER_SAMPLE_PERIOD_MS);
    }
}

int power_module_start(void)
{
    if (tx_mutex_create(&power_lock, "pwr", TX_INHERIT) != TX_SUCCESS)
        return -1;
    (void)memset(&snap, 0, sizeof(snap));
    (void)memset(&shp_snap, 0, sizeof(shp_snap));
    if (tx_thread_create(&power_thread, "power", power_thread_entry, 0, power_thread_stack, sizeof(power_thread_stack),
                         POWER_THREAD_PRIORITY, POWER_THREAD_PRIORITY, TX_NO_TIME_SLICE, TX_AUTO_START) != TX_SUCCESS)
        return -1;
    return 0;
}
