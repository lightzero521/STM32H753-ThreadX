#include "modules/power/bq25756/bq25756.h"

#include "board.h"

static bq25756_status update8(const bq25756 *device, uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t current;

    if (bq25756_read8(device, reg, &current) != BQ25756_OK)
        return BQ25756_EIO;
    current = (uint8_t)((current & (uint8_t)~mask) | (value & mask));
    return bq25756_write8(device, reg, current);
}

static bq25756_status encode_current(uint32_t current_ma, uint16_t sense_mohm, uint16_t min_code, uint16_t max_code,
                                     uint16_t *value)
{
    uint32_t code;

    if (sense_mohm == 0U)
        sense_mohm = 5U;
    if (sense_mohm > 100U || current_ma > 100000U)
        return BQ25756_EINVAL;
    code = (current_ma * sense_mohm + 125U) / 250U;
    if (code < min_code || code > max_code)
        return BQ25756_EINVAL;
    *value = (uint16_t)(code << 2);
    return BQ25756_OK;
}

static bq25756_status encode_voltage_20mv(uint32_t voltage_mv, uint32_t min_mv, uint32_t max_mv, uint16_t *value)
{
    if (voltage_mv < min_mv || voltage_mv > max_mv || (voltage_mv % 20U) != 0U)
        return BQ25756_EINVAL;
    *value = (uint16_t)((voltage_mv / 20U) << 2);
    return BQ25756_OK;
}

bq25756_status bq25756_read8(const bq25756 *device, uint8_t reg, uint8_t *value)
{
    if (device == 0 || value == 0)
        return BQ25756_EINVAL;
    (void)device->i2c_inst;
    return board_i2c_write_read(device->address, &reg, 1U, value, 1U, 50U) == 0 ? BQ25756_OK : BQ25756_EIO;
}

bq25756_status bq25756_write8(const bq25756 *device, uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};

    if (device == 0)
        return BQ25756_EINVAL;
    (void)device->i2c_inst;
    return board_i2c_write(device->address, data, sizeof(data), 50U) == 0 ? BQ25756_OK : BQ25756_EIO;
}

bq25756_status bq25756_read16(const bq25756 *device, uint8_t reg, uint16_t *value)
{
    uint8_t data[2];

    if (device == 0 || value == 0)
        return BQ25756_EINVAL;
    (void)device->i2c_inst;
    if (board_i2c_write_read(device->address, &reg, 1U, data, sizeof(data), 50U) != 0)
        return BQ25756_EIO;
    *value = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    return BQ25756_OK;
}

bq25756_status bq25756_write16(const bq25756 *device, uint8_t reg, uint16_t value)
{
    uint8_t data[3] = {reg, (uint8_t)value, (uint8_t)(value >> 8)};

    if (device == 0)
        return BQ25756_EINVAL;
    (void)device->i2c_inst;
    return board_i2c_write(device->address, data, sizeof(data), 50U) == 0 ? BQ25756_OK : BQ25756_EIO;
}

bq25756_status bq25756_probe(const bq25756 *device, uint8_t *part_info)
{
    uint8_t value;
    bq25756_status result = bq25756_read8(device, BQ25756_REG_PART_INFO, &value);

    if (result != BQ25756_OK)
        return result;
    if (part_info != 0)
        *part_info = value;
    return ((value >> 3) & 0x0FU) == BQ25756_PART_NUMBER ? BQ25756_OK : BQ25756_ENODEV;
}

bq25756_status bq25756_reset_registers(const bq25756 *device)
{
    return update8(device, BQ25756_REG_POWER_CONTROL, 0x80U, 0x80U);
}

bq25756_status bq25756_configure_pins(const bq25756 *device, const bq25756_pin_config *config)
{
    uint8_t pin_control;
    bq25756_status result;

    if (device == 0 || config == 0)
        return BQ25756_EINVAL;
    pin_control = (uint8_t)((config->ichg_pin_enabled ? 0x80U : 0U) | (config->ilim_hiz_pin_enabled ? 0x40U : 0U) |
                            (config->pg_pin_enabled ? 0U : 0x20U) | (config->stat_pins_enabled ? 0U : 0x10U));
    result = bq25756_write8(device, BQ25756_REG_PIN_CONTROL, pin_control);
    if (result != BQ25756_OK)
        return result;
    return update8(device, BQ25756_REG_CHARGER_CONTROL, 0x10U, config->ce_pin_enabled ? 0U : 0x10U);
}

bq25756_status bq25756_configure_software_host(const bq25756 *device)
{
    static const bq25756_pin_config pins = {.ichg_pin_enabled = false,
                                            .ilim_hiz_pin_enabled = false,
                                            .ce_pin_enabled = false,
                                            .stat_pins_enabled = false,
                                            .pg_pin_enabled = false};
    bq25756_status result;

    if (device == 0)
        return BQ25756_EINVAL;
    result = bq25756_set_watchdog(device, BQ25756_WATCHDOG_DISABLED);
    if (result != BQ25756_OK)
        return result;
    result = bq25756_configure_pins(device, &pins);
    if (result != BQ25756_OK)
        return result;
    result = bq25756_set_ts_enabled(device, false);
    if (result != BQ25756_OK)
        return result;
    result = bq25756_set_hiz(device, false);
    if (result != BQ25756_OK)
        return result;
    result = bq25756_set_reverse_enabled(device, false);
    if (result != BQ25756_OK)
        return result;
    result = bq25756_set_mppt_enabled(device, false);
    if (result != BQ25756_OK)
        return result;
    return bq25756_set_charge_enabled(device, false);
}

bq25756_status bq25756_configure_charging(const bq25756 *device, const bq25756_charge_config *config)
{
    uint16_t fb, charge, input, input_voltage, precharge, termination;
    bq25756_status result;

    if (device == 0 || config == 0 || config->fb_voltage_mv < 1504U || config->fb_voltage_mv > 1566U ||
        ((config->fb_voltage_mv - 1504U) & 1U) != 0U)
        return BQ25756_EINVAL;

    fb = (uint16_t)((config->fb_voltage_mv - 1504U) / 2U);
    result = encode_voltage_20mv(config->input_voltage_mv, 4200U, 65000U, &input_voltage);
    if (result != BQ25756_OK)
        return result;
    result = encode_current(config->charge_current_ma, device->battery_sense_mohm, 0x08U, 0x190U, &charge);
    if (result != BQ25756_OK)
        return result;
    result = encode_current(config->input_current_ma, device->input_sense_mohm, 0x08U, 0x190U, &input);
    if (result != BQ25756_OK)
        return result;
    result = encode_current(config->precharge_current_ma, device->battery_sense_mohm, 0x05U, 0xC8U, &precharge);
    if (result != BQ25756_OK)
        return result;
    result = encode_current(config->termination_current_ma, device->battery_sense_mohm, 0x05U, 0xC8U, &termination);
    if (result != BQ25756_OK)
        return result;

    if (bq25756_write16(device, BQ25756_REG_CHARGE_VOLTAGE, fb) != BQ25756_OK ||
        bq25756_write16(device, BQ25756_REG_CHARGE_CURRENT, charge) != BQ25756_OK ||
        bq25756_write16(device, BQ25756_REG_INPUT_CURRENT, input) != BQ25756_OK ||
        bq25756_write16(device, BQ25756_REG_INPUT_VOLTAGE, input_voltage) != BQ25756_OK ||
        bq25756_write16(device, BQ25756_REG_PRECHARGE_CURRENT, precharge) != BQ25756_OK ||
        bq25756_write16(device, BQ25756_REG_TERMINATION_CURRENT, termination) != BQ25756_OK)
        return BQ25756_EIO;
    return update8(device, BQ25756_REG_PRECHARGE_CONTROL, 0x09U,
                   (uint8_t)((config->enable_termination ? 0x08U : 0U) | (config->enable_precharge ? 0x01U : 0U)));
}

bq25756_status bq25756_set_charge_enabled(const bq25756 *device, bool enabled)
{
    return update8(device, BQ25756_REG_CHARGER_CONTROL, 0x01U, enabled ? 0x01U : 0U);
}

bq25756_status bq25756_disable_charging(const bq25756 *device)
{
    bq25756_status result;

    result = bq25756_set_charge_enabled(device, false);
    if (result != BQ25756_OK)
        return result;
    result = bq25756_set_hiz(device, false);
    if (result != BQ25756_OK)
        return result;
    result = bq25756_set_reverse_enabled(device, false);
    if (result != BQ25756_OK)
        return result;
    return bq25756_set_watchdog(device, BQ25756_WATCHDOG_DISABLED);
}

bq25756_status bq25756_set_hiz(const bq25756 *device, bool enabled)
{
    return update8(device, BQ25756_REG_CHARGER_CONTROL, 0x04U, enabled ? 0x04U : 0U);
}

bq25756_status bq25756_set_vbat_lowv(const bq25756 *device, bq25756_vbat_lowv threshold)
{
    if ((uint32_t)threshold > (uint32_t)BQ25756_VBAT_LOWV_714)
        return BQ25756_EINVAL;
    return update8(device, BQ25756_REG_PRECHARGE_CONTROL, 0x06U, (uint8_t)((uint8_t)threshold << 1));
}

bq25756_status bq25756_set_recharge_threshold(const bq25756 *device, bq25756_vrechg threshold)
{
    if ((uint32_t)threshold > (uint32_t)BQ25756_VRECHG_976)
        return BQ25756_EINVAL;
    return update8(device, BQ25756_REG_CHARGER_CONTROL, 0xC0U, (uint8_t)((uint8_t)threshold << 6));
}

bq25756_status bq25756_set_safety_timer(const bq25756 *device, bq25756_safety_timer timer, bool enabled)
{
    if ((uint32_t)timer > (uint32_t)BQ25756_SAFETY_TIMER_24H)
        return BQ25756_EINVAL;
    return update8(device, BQ25756_REG_TIMER_CONTROL, 0x0EU, (uint8_t)((enabled ? 0x08U : 0U) | ((uint8_t)timer << 1)));
}

bq25756_status bq25756_set_topoff_timer(const bq25756 *device, bq25756_topoff_timer timer)
{
    if ((uint32_t)timer > (uint32_t)BQ25756_TOPOFF_45MIN)
        return BQ25756_EINVAL;
    return update8(device, BQ25756_REG_TIMER_CONTROL, 0xC0U, (uint8_t)((uint8_t)timer << 6));
}

bq25756_status bq25756_configure_reverse(const bq25756 *device, const bq25756_reverse_config *config)
{
    uint16_t vac, iac;
    bq25756_status result;

    if (device == 0 || config == 0 || (uint32_t)config->ibat_rev > (uint32_t)BQ25756_IBAT_REV_5A)
        return BQ25756_EINVAL;
    result = encode_voltage_20mv(config->vac_mv, 3300U, 65000U, &vac);
    if (result != BQ25756_OK)
        return result;
    result = encode_current(config->iac_ma, device->input_sense_mohm, 0x08U, 0x190U, &iac);
    if (result != BQ25756_OK)
        return result;
    if (bq25756_write16(device, BQ25756_REG_REVERSE_VOLTAGE, vac) != BQ25756_OK ||
        bq25756_write16(device, BQ25756_REG_REVERSE_CURRENT, iac) != BQ25756_OK)
        return BQ25756_EIO;
    result = update8(device, BQ25756_REG_REVERSE_BAT_CURRENT, 0xC0U, (uint8_t)((uint8_t)config->ibat_rev << 6));
    if (result != BQ25756_OK)
        return result;
    return bq25756_set_reverse_uvp(device, config->uvp_fixed_3v3);
}

bq25756_status bq25756_set_reverse_enabled(const bq25756 *device, bool enabled)
{
    return update8(device, BQ25756_REG_POWER_CONTROL, 0x01U, enabled ? 0x01U : 0U);
}

bq25756_status bq25756_set_reverse_uvp(const bq25756 *device, bool fixed_3v3)
{
    return update8(device, BQ25756_REG_REVERSE_UNDERVOLTAGE, 0x20U, fixed_3v3 ? 0x20U : 0U);
}

bq25756_status bq25756_configure_mppt(const bq25756 *device, const bq25756_mppt_config *config)
{
    if (device == 0 || config == 0 || (uint32_t)config->perturb > (uint32_t)BQ25756_MPPT_PO_10S ||
        (uint32_t)config->full_sweep > (uint32_t)BQ25756_MPPT_SWEEP_20MIN)
        return BQ25756_EINVAL;
    return update8(device, BQ25756_REG_MPPT_CONTROL, 0x66U,
                   (uint8_t)(((uint8_t)config->perturb << 5) | ((uint8_t)config->full_sweep << 1)));
}

bq25756_status bq25756_set_mppt_enabled(const bq25756 *device, bool enabled)
{
    return update8(device, BQ25756_REG_MPPT_CONTROL, 0x01U, enabled ? 0x01U : 0U);
}

bq25756_status bq25756_force_mppt_sweep(const bq25756 *device)
{
    return update8(device, BQ25756_REG_MPPT_CONTROL, 0x80U, 0x80U);
}

bq25756_status bq25756_read_mppt_voltage(const bq25756 *device, uint32_t *vac_mv)
{
    uint16_t raw;

    if (device == 0 || vac_mv == 0)
        return BQ25756_EINVAL;
    if (bq25756_read16(device, BQ25756_REG_MPPT_VOLTAGE, &raw) != BQ25756_OK)
        return BQ25756_EIO;
    *vac_mv = ((uint32_t)(raw >> 2) & 0x0FFFU) * 20U;
    return BQ25756_OK;
}

bq25756_status bq25756_set_ts_enabled(const bq25756 *device, bool enabled)
{
    return update8(device, BQ25756_REG_TS_CHARGE_BEHAVIOR, 0x01U, enabled ? 0x01U : 0U);
}

bq25756_status bq25756_set_jeita_enabled(const bq25756 *device, bool enabled)
{
    return update8(device, BQ25756_REG_TS_CHARGE_BEHAVIOR, 0x02U, enabled ? 0x02U : 0U);
}

bq25756_status bq25756_set_watchdog(const bq25756 *device, bq25756_watchdog_period period)
{
    if ((uint32_t)period > (uint32_t)BQ25756_WATCHDOG_160S)
        return BQ25756_EINVAL;
    return update8(device, BQ25756_REG_TIMER_CONTROL, 0x30U, (uint8_t)((uint8_t)period << 4));
}

bq25756_status bq25756_kick_watchdog(const bq25756 *device)
{
    return update8(device, BQ25756_REG_CHARGER_CONTROL, 0x20U, 0x20U);
}

bq25756_status bq25756_set_pfm(const bq25756 *device, bool enabled)
{
    return update8(device, BQ25756_REG_POWER_CONTROL, 0x20U, enabled ? 0x20U : 0U);
}

bq25756_status bq25756_configure_gate_drive(const bq25756 *device, const bq25756_gate_drive_config *config)
{
    uint8_t strength;
    uint8_t dead_time;

    if (device == 0 || config == 0)
        return BQ25756_EINVAL;
    if ((uint32_t)config->buck_hs > (uint32_t)BQ25756_DRV_SLOWEST ||
        (uint32_t)config->buck_ls > (uint32_t)BQ25756_DRV_SLOWEST ||
        (uint32_t)config->boost_hs > (uint32_t)BQ25756_DRV_SLOWEST ||
        (uint32_t)config->boost_ls > (uint32_t)BQ25756_DRV_SLOWEST ||
        (uint32_t)config->buck_dead_time > (uint32_t)BQ25756_DEAD_TIME_135NS ||
        (uint32_t)config->boost_dead_time > (uint32_t)BQ25756_DEAD_TIME_135NS)
        return BQ25756_EINVAL;

    strength = (uint8_t)(((uint8_t)config->boost_hs << 6) | ((uint8_t)config->buck_hs << 4) |
                         ((uint8_t)config->boost_ls << 2) | (uint8_t)config->buck_ls);
    dead_time = (uint8_t)(((uint8_t)config->boost_dead_time << 2) | (uint8_t)config->buck_dead_time);
    if (bq25756_write8(device, BQ25756_REG_GATE_DRIVE_STRENGTH, strength) != BQ25756_OK)
        return BQ25756_EIO;
    return bq25756_write8(device, BQ25756_REG_GATE_DRIVE_DEAD_TIME, dead_time);
}

bq25756_status bq25756_read_status(const bq25756 *device, bq25756_status_snapshot *status)
{
    uint8_t reg = BQ25756_REG_STATUS1;

    if (device == 0 || status == 0)
        return BQ25756_EINVAL;
    (void)device->i2c_inst;
    return board_i2c_write_read(device->address, &reg, 1U, (uint8_t *)status, sizeof(*status), 50U) == 0 ? BQ25756_OK
                                                                                                        : BQ25756_EIO;
}

void bq25756_decode_status(const bq25756_status_snapshot *raw, bq25756_status_decoded *decoded)
{
    uint8_t ts;

    if (raw == 0 || decoded == 0)
        return;
    decoded->adc_done = (raw->status1 & 0x80U) != 0U;
    decoded->iac_dpm = (raw->status1 & 0x40U) != 0U;
    decoded->vac_dpm = (raw->status1 & 0x20U) != 0U;
    decoded->watchdog_expired = (raw->status1 & 0x08U) != 0U;
    decoded->charge = (bq25756_charge_phase)(raw->status1 & 0x07U);
    decoded->pg = (raw->status2 & 0x80U) != 0U;
    ts = (uint8_t)((raw->status2 >> 4) & 0x07U);
    decoded->ts = ts > (uint8_t)BQ25756_TS_HOT ? BQ25756_TS_NORMAL : (bq25756_ts_state)ts;
    decoded->mppt = (bq25756_mppt_state)(raw->status2 & 0x03U);
    decoded->cv_timer = (raw->status3 & 0x08U) != 0U;
    decoded->reverse = (raw->status3 & 0x04U) != 0U;
    decoded->vac_uv = (raw->fault & 0x80U) != 0U;
    decoded->vac_ov = (raw->fault & 0x40U) != 0U;
    decoded->ibat_ocp = (raw->fault & 0x20U) != 0U;
    decoded->vbat_ov = (raw->fault & 0x10U) != 0U;
    decoded->tshut = (raw->fault & 0x08U) != 0U;
    decoded->safety_timer = (raw->fault & 0x04U) != 0U;
    decoded->drv_fault = (raw->fault & 0x02U) != 0U;
}

bq25756_status bq25756_read_and_clear_flags(const bq25756 *device, bq25756_flags *flags)
{
    uint8_t reg = BQ25756_REG_FLAG1;

    if (device == 0 || flags == 0)
        return BQ25756_EINVAL;
    (void)device->i2c_inst;
    return board_i2c_write_read(device->address, &reg, 1U, (uint8_t *)flags, sizeof(*flags), 50U) == 0 ? BQ25756_OK
                                                                                                      : BQ25756_EIO;
}

bq25756_status bq25756_set_irq_mask(const bq25756 *device, const bq25756_irq_mask *mask)
{
    uint8_t data[4];

    if (device == 0 || mask == 0)
        return BQ25756_EINVAL;
    data[0] = BQ25756_REG_MASK1;
    data[1] = mask->charger1 & 0xE9U;
    data[2] = mask->charger2 & 0x9BU;
    data[3] = mask->fault & 0xFEU;
    (void)device->i2c_inst;
    return board_i2c_write(device->address, data, sizeof(data), 50U) == 0 ? BQ25756_OK : BQ25756_EIO;
}

void bq25756_irq_mask_debug(bq25756_irq_mask *mask)
{
    if (mask == 0)
        return;
    mask->charger1 = 0x88U;
    mask->charger2 = 0x1BU;
    mask->fault = 0x00U;
}

bq25756_status bq25756_adc_configure(const bq25756 *device, uint8_t channels, bool continuous)
{
    bq25756_status result;

    if (device == 0 || (channels & (uint8_t)~BQ25756_ADC_ALL) != 0U || channels == 0U)
        return BQ25756_EINVAL;
    result = bq25756_write8(device, BQ25756_REG_ADC_CHANNEL_CONTROL,
                            (uint8_t)(0x08U | ((uint8_t)~channels & BQ25756_ADC_ALL)));
    if (result != BQ25756_OK)
        return result;
    return update8(device, BQ25756_REG_ADC_CONTROL, 0xFCU, (uint8_t)(0x20U | (continuous ? 0U : 0x40U)));
}

bq25756_status bq25756_adc_start(const bq25756 *device)
{
    return update8(device, BQ25756_REG_ADC_CONTROL, 0x80U, 0x80U);
}

bq25756_status bq25756_adc_wait_done(const bq25756 *device, uint32_t attempts)
{
    uint8_t status1;
    uint32_t i;

    if (device == 0 || attempts == 0U)
        return BQ25756_EINVAL;
    for (i = 0U; i < attempts; i++) {
        if (bq25756_read8(device, BQ25756_REG_STATUS1, &status1) != BQ25756_OK)
            return BQ25756_EIO;
        if ((status1 & 0x80U) != 0U)
            return BQ25756_OK;
    }
    return BQ25756_ETIMEOUT;
}

bq25756_status bq25756_adc_read(const bq25756 *device, bq25756_adc_values *values)
{
    uint16_t raw_iac, raw_ibat, raw_vac, raw_vbat, raw_ts, raw_vfb;
    uint16_t input_sense, battery_sense;

    if (device == 0 || values == 0)
        return BQ25756_EINVAL;
    if (bq25756_read16(device, BQ25756_REG_IAC_ADC, &raw_iac) != BQ25756_OK ||
        bq25756_read16(device, BQ25756_REG_IBAT_ADC, &raw_ibat) != BQ25756_OK ||
        bq25756_read16(device, BQ25756_REG_VAC_ADC, &raw_vac) != BQ25756_OK ||
        bq25756_read16(device, BQ25756_REG_VBAT_ADC, &raw_vbat) != BQ25756_OK ||
        bq25756_read16(device, BQ25756_REG_TS_ADC, &raw_ts) != BQ25756_OK ||
        bq25756_read16(device, BQ25756_REG_VFB_ADC, &raw_vfb) != BQ25756_OK)
        return BQ25756_EIO;

    input_sense = device->input_sense_mohm == 0U ? 5U : device->input_sense_mohm;
    battery_sense = device->battery_sense_mohm == 0U ? 5U : device->battery_sense_mohm;
    values->input_current_ma_x10 = (int32_t)(int16_t)raw_iac * 40 / input_sense;
    values->battery_current_ma = (int32_t)(int16_t)raw_ibat * 10 / battery_sense;
    values->input_voltage_mv = (uint32_t)(raw_vac & 0x7FFFU) * 2U;
    values->battery_voltage_mv = (uint32_t)(raw_vbat & 0x7FFFU) * 2U;
    values->ts_permille = (uint16_t)(((uint32_t)(raw_ts & 0x03FFU) * 1000U) / 1024U);
    values->feedback_voltage_mv = raw_vfb & 0x07FFU;
    return BQ25756_OK;
}

const char *bq25756_charge_phase_name(bq25756_charge_phase phase)
{
    static const char *const names[] = {"idle", "trickle", "precharge", "cc", "cv", "reserved", "topoff", "done"};

    if ((uint32_t)phase >= (uint32_t)(sizeof(names) / sizeof(names[0])))
        return "unknown";
    return names[phase];
}

const char *bq25756_ts_state_name(bq25756_ts_state state)
{
    static const char *const names[] = {"normal", "warm", "cool", "cold", "hot"};

    if ((uint32_t)state >= (uint32_t)(sizeof(names) / sizeof(names[0])))
        return "unknown";
    return names[state];
}

const char *bq25756_mppt_state_name(bq25756_mppt_state state)
{
    static const char *const names[] = {"off", "idle", "sweep", "locked"};

    if ((uint32_t)state >= (uint32_t)(sizeof(names) / sizeof(names[0])))
        return "unknown";
    return names[state];
}
