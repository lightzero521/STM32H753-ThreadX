#ifndef MODULES_BQ25756_H
#define MODULES_BQ25756_H

#include <stdbool.h>
#include <stdint.h>

#define BQ25756_I2C_ADDRESS 0x6BU
#define BQ25756_PART_NUMBER 0x02U

#define BQ25756_REG_CHARGE_VOLTAGE 0x00U
#define BQ25756_REG_CHARGE_CURRENT 0x02U
#define BQ25756_REG_INPUT_CURRENT 0x06U
#define BQ25756_REG_INPUT_VOLTAGE 0x08U
#define BQ25756_REG_REVERSE_CURRENT 0x0AU
#define BQ25756_REG_REVERSE_VOLTAGE 0x0CU
#define BQ25756_REG_PRECHARGE_CURRENT 0x10U
#define BQ25756_REG_TERMINATION_CURRENT 0x12U
#define BQ25756_REG_PRECHARGE_CONTROL 0x14U
#define BQ25756_REG_TIMER_CONTROL 0x15U
#define BQ25756_REG_CHARGER_CONTROL 0x17U
#define BQ25756_REG_PIN_CONTROL 0x18U
#define BQ25756_REG_POWER_CONTROL 0x19U
#define BQ25756_REG_MPPT_CONTROL 0x1AU
#define BQ25756_REG_TS_CHARGE_THRESHOLD 0x1BU
#define BQ25756_REG_TS_CHARGE_BEHAVIOR 0x1CU
#define BQ25756_REG_TS_REVERSE_THRESHOLD 0x1DU
#define BQ25756_REG_REVERSE_UNDERVOLTAGE 0x1EU
#define BQ25756_REG_MPPT_VOLTAGE 0x1FU
#define BQ25756_REG_STATUS1 0x21U
#define BQ25756_REG_STATUS2 0x22U
#define BQ25756_REG_STATUS3 0x23U
#define BQ25756_REG_FAULT 0x24U
#define BQ25756_REG_FLAG1 0x25U
#define BQ25756_REG_FLAG2 0x26U
#define BQ25756_REG_FAULT_FLAG 0x27U
#define BQ25756_REG_MASK1 0x28U
#define BQ25756_REG_MASK2 0x29U
#define BQ25756_REG_FAULT_MASK 0x2AU
#define BQ25756_REG_ADC_CONTROL 0x2BU
#define BQ25756_REG_ADC_CHANNEL_CONTROL 0x2CU
#define BQ25756_REG_IAC_ADC 0x2DU
#define BQ25756_REG_IBAT_ADC 0x2FU
#define BQ25756_REG_VAC_ADC 0x31U
#define BQ25756_REG_VBAT_ADC 0x33U
#define BQ25756_REG_TS_ADC 0x37U
#define BQ25756_REG_VFB_ADC 0x39U
#define BQ25756_REG_GATE_DRIVE_STRENGTH 0x3BU
#define BQ25756_REG_GATE_DRIVE_DEAD_TIME 0x3CU
#define BQ25756_REG_PART_INFO 0x3DU
#define BQ25756_REG_REVERSE_BAT_CURRENT 0x62U

#define BQ25756_ADC_IAC (1U << 7)
#define BQ25756_ADC_IBAT (1U << 6)
#define BQ25756_ADC_VAC (1U << 5)
#define BQ25756_ADC_VBAT (1U << 4)
#define BQ25756_ADC_TS (1U << 2)
#define BQ25756_ADC_VFB (1U << 1)
#define BQ25756_ADC_ALL 0xF6U
#define BQ25756_ADC_MONITOR (BQ25756_ADC_IAC | BQ25756_ADC_IBAT | BQ25756_ADC_VAC | BQ25756_ADC_VBAT)

typedef enum {
    BQ25756_OK = 0,
    BQ25756_EINVAL = -1,
    BQ25756_EIO = -2,
    BQ25756_ENODEV = -3,
    BQ25756_ETIMEOUT = -4
} bq25756_status;

typedef enum {
    BQ25756_WATCHDOG_DISABLED = 0,
    BQ25756_WATCHDOG_40S,
    BQ25756_WATCHDOG_80S,
    BQ25756_WATCHDOG_160S
} bq25756_watchdog_period;

typedef enum {
    BQ25756_SAFETY_TIMER_5H = 0,
    BQ25756_SAFETY_TIMER_8H,
    BQ25756_SAFETY_TIMER_12H,
    BQ25756_SAFETY_TIMER_24H
} bq25756_safety_timer;

typedef enum {
    BQ25756_TOPOFF_DISABLED = 0,
    BQ25756_TOPOFF_15MIN,
    BQ25756_TOPOFF_30MIN,
    BQ25756_TOPOFF_45MIN
} bq25756_topoff_timer;

typedef enum {
    BQ25756_VBAT_LOWV_30 = 0,
    BQ25756_VBAT_LOWV_55,
    BQ25756_VBAT_LOWV_667,
    BQ25756_VBAT_LOWV_714
} bq25756_vbat_lowv;

typedef enum { BQ25756_VRECHG_93 = 0, BQ25756_VRECHG_943, BQ25756_VRECHG_952, BQ25756_VRECHG_976 } bq25756_vrechg;

typedef enum {
    BQ25756_CHARGE_NOT_CHARGING = 0,
    BQ25756_CHARGE_TRICKLE,
    BQ25756_CHARGE_PRECHARGE,
    BQ25756_CHARGE_FAST_CC,
    BQ25756_CHARGE_TAPER_CV,
    BQ25756_CHARGE_RESERVED,
    BQ25756_CHARGE_TOPOFF,
    BQ25756_CHARGE_DONE
} bq25756_charge_phase;

typedef enum {
    BQ25756_TS_NORMAL = 0,
    BQ25756_TS_WARM,
    BQ25756_TS_COOL,
    BQ25756_TS_COLD,
    BQ25756_TS_HOT
} bq25756_ts_state;

typedef enum {
    BQ25756_IBAT_REV_20A = 0,
    BQ25756_IBAT_REV_15A,
    BQ25756_IBAT_REV_10A,
    BQ25756_IBAT_REV_5A
} bq25756_ibat_rev;

typedef enum {
    BQ25756_MPPT_DISABLED = 0,
    BQ25756_MPPT_IDLE,
    BQ25756_MPPT_SWEEP,
    BQ25756_MPPT_LOCKED
} bq25756_mppt_state;

typedef enum {
    BQ25756_MPPT_PO_OFF = 0,
    BQ25756_MPPT_PO_500MS,
    BQ25756_MPPT_PO_1S,
    BQ25756_MPPT_PO_10S
} bq25756_mppt_po_timer;

typedef enum {
    BQ25756_MPPT_SWEEP_3MIN = 0,
    BQ25756_MPPT_SWEEP_10MIN,
    BQ25756_MPPT_SWEEP_15MIN,
    BQ25756_MPPT_SWEEP_20MIN
} bq25756_mppt_sweep_timer;

typedef enum {
    BQ25756_DRV_FASTEST = 0,
    BQ25756_DRV_FASTER,
    BQ25756_DRV_SLOWER,
    BQ25756_DRV_SLOWEST
} bq25756_drv_strength;

typedef enum {
    BQ25756_DEAD_TIME_45NS = 0,
    BQ25756_DEAD_TIME_75NS,
    BQ25756_DEAD_TIME_105NS,
    BQ25756_DEAD_TIME_135NS
} bq25756_dead_time;

typedef struct {
    uint8_t i2c_inst;
    uint8_t address;
    uint16_t input_sense_mohm;
    uint16_t battery_sense_mohm;
    uint32_t timeout_cycles;
} bq25756;

typedef struct {
    uint16_t fb_voltage_mv;
    uint32_t charge_current_ma;
    uint32_t input_current_ma;
    uint32_t input_voltage_mv;
    uint32_t precharge_current_ma;
    uint32_t termination_current_ma;
    bool enable_precharge;
    bool enable_termination;
} bq25756_charge_config;

typedef struct {
    bool ichg_pin_enabled;
    bool ilim_hiz_pin_enabled;
    bool ce_pin_enabled;
    bool stat_pins_enabled;
    bool pg_pin_enabled;
} bq25756_pin_config;

typedef struct {
    uint32_t vac_mv;
    uint32_t iac_ma;
    bq25756_ibat_rev ibat_rev;
    bool uvp_fixed_3v3;
} bq25756_reverse_config;

typedef struct {
    bq25756_mppt_po_timer perturb;
    bq25756_mppt_sweep_timer full_sweep;
} bq25756_mppt_config;

typedef struct {
    bq25756_drv_strength buck_hs;
    bq25756_drv_strength buck_ls;
    bq25756_drv_strength boost_hs;
    bq25756_drv_strength boost_ls;
    bq25756_dead_time buck_dead_time;
    bq25756_dead_time boost_dead_time;
} bq25756_gate_drive_config;

typedef struct {
    uint8_t status1;
    uint8_t status2;
    uint8_t status3;
    uint8_t fault;
} bq25756_status_snapshot;

typedef struct {
    bq25756_charge_phase charge;
    bq25756_ts_state ts;
    bq25756_mppt_state mppt;
    bool adc_done;
    bool iac_dpm;
    bool vac_dpm;
    bool watchdog_expired;
    bool pg;
    bool reverse;
    bool cv_timer;
    bool vac_uv;
    bool vac_ov;
    bool ibat_ocp;
    bool vbat_ov;
    bool tshut;
    bool safety_timer;
    bool drv_fault;
} bq25756_status_decoded;

typedef struct {
    uint8_t charger1;
    uint8_t charger2;
    uint8_t fault;
} bq25756_flags;

typedef struct {
    uint8_t charger1;
    uint8_t charger2;
    uint8_t fault;
} bq25756_irq_mask;

typedef struct {
    int32_t input_current_ma_x10;
    int32_t battery_current_ma;
    uint32_t input_voltage_mv;
    uint32_t battery_voltage_mv;
    uint16_t ts_permille;
    uint16_t feedback_voltage_mv;
} bq25756_adc_values;

/* Bus */
bq25756_status bq25756_read8(const bq25756 *device, uint8_t reg, uint8_t *value);
bq25756_status bq25756_write8(const bq25756 *device, uint8_t reg, uint8_t value);
bq25756_status bq25756_read16(const bq25756 *device, uint8_t reg, uint16_t *value);
bq25756_status bq25756_write16(const bq25756 *device, uint8_t reg, uint16_t value);

/* Device */
bq25756_status bq25756_probe(const bq25756 *device, uint8_t *part_info);
bq25756_status bq25756_reset_registers(const bq25756 *device);

/* Pins */
bq25756_status bq25756_configure_pins(const bq25756 *device, const bq25756_pin_config *config);
bq25756_status bq25756_configure_software_host(const bq25756 *device);

/* Charge */
bq25756_status bq25756_configure_charging(const bq25756 *device, const bq25756_charge_config *config);
bq25756_status bq25756_set_charge_enabled(const bq25756 *device, bool enabled);
bq25756_status bq25756_disable_charging(const bq25756 *device);
bq25756_status bq25756_set_hiz(const bq25756 *device, bool enabled);
bq25756_status bq25756_set_vbat_lowv(const bq25756 *device, bq25756_vbat_lowv threshold);
bq25756_status bq25756_set_recharge_threshold(const bq25756 *device, bq25756_vrechg threshold);
bq25756_status bq25756_set_safety_timer(const bq25756 *device, bq25756_safety_timer timer, bool enabled);
bq25756_status bq25756_set_topoff_timer(const bq25756 *device, bq25756_topoff_timer timer);

/* Reverse */
bq25756_status bq25756_configure_reverse(const bq25756 *device, const bq25756_reverse_config *config);
bq25756_status bq25756_set_reverse_enabled(const bq25756 *device, bool enabled);
bq25756_status bq25756_set_reverse_uvp(const bq25756 *device, bool fixed_3v3);

/* MPPT */
bq25756_status bq25756_configure_mppt(const bq25756 *device, const bq25756_mppt_config *config);
bq25756_status bq25756_set_mppt_enabled(const bq25756 *device, bool enabled);
bq25756_status bq25756_force_mppt_sweep(const bq25756 *device);
bq25756_status bq25756_read_mppt_voltage(const bq25756 *device, uint32_t *vac_mv);

/* TS */
bq25756_status bq25756_set_ts_enabled(const bq25756 *device, bool enabled);
bq25756_status bq25756_set_jeita_enabled(const bq25756 *device, bool enabled);

/* Watchdog */
bq25756_status bq25756_set_watchdog(const bq25756 *device, bq25756_watchdog_period period);
bq25756_status bq25756_kick_watchdog(const bq25756 *device);

/* Converter */
bq25756_status bq25756_set_pfm(const bq25756 *device, bool enabled);
bq25756_status bq25756_configure_gate_drive(const bq25756 *device, const bq25756_gate_drive_config *config);

/* Status and IRQ */
bq25756_status bq25756_read_status(const bq25756 *device, bq25756_status_snapshot *status);
void bq25756_decode_status(const bq25756_status_snapshot *raw, bq25756_status_decoded *decoded);
bq25756_status bq25756_read_and_clear_flags(const bq25756 *device, bq25756_flags *flags);
bq25756_status bq25756_set_irq_mask(const bq25756 *device, const bq25756_irq_mask *mask);
void bq25756_irq_mask_debug(bq25756_irq_mask *mask);

/* ADC */
bq25756_status bq25756_adc_configure(const bq25756 *device, uint8_t channels, bool continuous);
bq25756_status bq25756_adc_start(const bq25756 *device);
bq25756_status bq25756_adc_wait_done(const bq25756 *device, uint32_t attempts);
bq25756_status bq25756_adc_read(const bq25756 *device, bq25756_adc_values *values);

const char *bq25756_charge_phase_name(bq25756_charge_phase phase);
const char *bq25756_ts_state_name(bq25756_ts_state state);
const char *bq25756_mppt_state_name(bq25756_mppt_state state);

#endif
