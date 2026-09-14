#include "modules/power/shp8808/shp8808.h"

#include "board.h"

#define SHP8808_IBAT_REF_MOHM 5U
#define SHP8808_IBUS_REF_MOHM 2U
#define SHP8808_ICHG_OFFSET_MA 1000U
#define SHP8808_ICHG_STEP_MA 100U
#define SHP8808_IPRE_OFFSET_MA 400U
#define SHP8808_IPRE_STEP_MA 100U
#define SHP8808_ITERM_OFFSET_MA 400U
#define SHP8808_ITERM_STEP_MA 100U
#define SHP8808_IBUS_OFFSET_MA 2375U
#define SHP8808_IBUS_STEP_MA 375U
#define SHP8808_ICHG_MAX_MA 20000U
#define SHP8808_IPRE_MAX_MA 10000U
#define SHP8808_ITERM_MAX_MA 6700U
#define SHP8808_IBUS_MAX_MA 50000U
#define SHP8808_ADC_IBUS_LSB_MA 25U
#define SHP8808_ADC_IBAT_LSB_MA 10U

static uint16_t input_sense(const shp8808 *device)
{
    return (device == 0 || device->input_sense_mohm == 0U) ? SHP8808_IBUS_REF_MOHM : device->input_sense_mohm;
}

static uint16_t battery_sense(const shp8808 *device)
{
    return (device == 0 || device->battery_sense_mohm == 0U) ? SHP8808_IBAT_REF_MOHM : device->battery_sense_mohm;
}

static shp8808_status update8(const shp8808 *device, uint8_t reg, uint8_t mask, uint8_t value)
{
    uint8_t current;

    if (shp8808_read8(device, reg, &current) != SHP8808_OK)
        return SHP8808_EIO;
    current = (uint8_t)((current & (uint8_t)~mask) | (value & mask));
    return shp8808_write8(device, reg, current);
}

static shp8808_status encode_current(uint32_t current_ma, uint16_t sense_mohm, uint32_t offset_ma, uint32_t step_ma,
                                     uint16_t ref_mohm, uint16_t max_code, uint32_t max_ma, uint8_t *value)
{
    uint32_t drop;
    uint32_t step;
    uint32_t offset;
    uint32_t code;

    if (sense_mohm == 0U || sense_mohm > 100U || step_ma == 0U || current_ma > 100000U)
        return SHP8808_EINVAL;
    drop = current_ma * sense_mohm;
    offset = offset_ma * ref_mohm;
    step = step_ma * ref_mohm;
    if (drop < offset || current_ma > (max_ma * ref_mohm) / sense_mohm)
        return SHP8808_EINVAL;
    code = (drop - offset + (step / 2U)) / step;
    if (code > max_code)
        return SHP8808_EINVAL;
    *value = (uint8_t)code;
    return SHP8808_OK;
}

static uint32_t decode_adc_current(uint16_t raw, uint16_t sense_mohm, uint16_t ref_mohm, uint32_t lsb_ma)
{
    uint32_t code = (uint32_t)raw & 0x07FFU;

    if (sense_mohm == 0U)
        sense_mohm = ref_mohm;
    return (code * lsb_ma * ref_mohm) / sense_mohm;
}

static shp8808_status encode_otg_voltage(uint32_t voltage_mv, uint16_t *value)
{
    uint32_t code;

    if (voltage_mv >= 5000U && voltage_mv <= 21000U) {
        if (((voltage_mv - 540U) % 20U) != 0U)
            return SHP8808_EINVAL;
        code = (voltage_mv - 540U) / 20U;
        if (code > 0x03FFU)
            return SHP8808_EINVAL;
        *value = (uint16_t)code;
        return SHP8808_OK;
    }
    if (voltage_mv >= 21050U && voltage_mv <= 48000U) {
        if (((voltage_mv - 21050U) % 50U) != 0U)
            return SHP8808_EINVAL;
        code = (voltage_mv - 21050U) / 50U;
        if (code > 0x03FFU)
            return SHP8808_EINVAL;
        *value = (uint16_t)(0x0400U | code);
        return SHP8808_OK;
    }
    return SHP8808_EINVAL;
}

shp8808_status shp8808_read8(const shp8808 *device, uint8_t reg, uint8_t *value)
{
    if (device == 0 || value == 0)
        return SHP8808_EINVAL;
    (void)device->i2c_inst;
    return board_i2c_write_read(device->address, &reg, 1U, value, 1U, 50U) == 0 ? SHP8808_OK : SHP8808_EIO;
}

shp8808_status shp8808_write8(const shp8808 *device, uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};

    if (device == 0)
        return SHP8808_EINVAL;
    (void)device->i2c_inst;
    return board_i2c_write(device->address, data, sizeof(data), 50U) == 0 ? SHP8808_OK : SHP8808_EIO;
}

shp8808_status shp8808_read16(const shp8808 *device, uint8_t reg, uint16_t *value)
{
    uint8_t data[2];

    if (device == 0 || value == 0)
        return SHP8808_EINVAL;
    (void)device->i2c_inst;
    if (board_i2c_write_read(device->address, &reg, 1U, data, sizeof(data), 50U) != 0)
        return SHP8808_EIO;
    *value = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    return SHP8808_OK;
}

shp8808_status shp8808_write16(const shp8808 *device, uint8_t reg, uint16_t value)
{
    uint8_t data[3] = {reg, (uint8_t)value, (uint8_t)(value >> 8)};

    if (device == 0)
        return SHP8808_EINVAL;
    (void)device->i2c_inst;
    return board_i2c_write(device->address, data, sizeof(data), 50U) == 0 ? SHP8808_OK : SHP8808_EIO;
}

shp8808_status shp8808_probe(const shp8808 *device, uint8_t *option0)
{
    uint8_t value;
    uint8_t status0;
    shp8808_status result;

    result = shp8808_read8(device, SHP8808_REG_CHARGE_OPTION0, &value);
    if (result != SHP8808_OK)
        return result;
    result = shp8808_read8(device, SHP8808_REG_STATUS0, &status0);
    if (result != SHP8808_OK)
        return result;
    if (option0 != 0)
        *option0 = value;
    return SHP8808_OK;
}

shp8808_status shp8808_reset_registers(const shp8808 *device)
{
    return update8(device, SHP8808_REG_CHARGE_OPTION0, 0x01U, 0x01U);
}

shp8808_status shp8808_configure_pins(const shp8808 *device, const shp8808_pin_config *config)
{
    uint8_t option1;
    shp8808_status result;

    if (device == 0 || config == 0)
        return SHP8808_EINVAL;
    option1 = (uint8_t)((config->ibus_pin_enabled ? 0x80U : 0U) | (config->ibat_pin_enabled ? 0x40U : 0U));
    result = update8(device, SHP8808_REG_CHARGE_OPTION1, 0xC0U, option1);
    if (result != SHP8808_OK)
        return result;
    return update8(device, SHP8808_REG_CHARGE_OPTION4, 0x40U, config->otg_fb_pin_enabled ? 0x40U : 0U);
}

shp8808_status shp8808_configure_software_host(const shp8808 *device)
{
    static const shp8808_pin_config pins = {
        .ibus_pin_enabled = false, .ibat_pin_enabled = false, .otg_fb_pin_enabled = false};
    shp8808_status result;

    if (device == 0)
        return SHP8808_EINVAL;
    result = shp8808_configure_pins(device, &pins);
    if (result != SHP8808_OK)
        return result;
    result = shp8808_set_ts_enabled(device, false);
    if (result != SHP8808_OK)
        return result;
    result = shp8808_set_jeita_enabled(device, false);
    if (result != SHP8808_OK)
        return result;
    result = shp8808_set_hiz(device, false);
    if (result != SHP8808_OK)
        return result;
    result = shp8808_set_reverse_enabled(device, false);
    if (result != SHP8808_OK)
        return result;
    result = shp8808_set_mppt_enabled(device, false);
    if (result != SHP8808_OK)
        return result;
    return shp8808_set_charge_enabled(device, false);
}

shp8808_status shp8808_configure_charging(const shp8808 *device, const shp8808_charge_config *config)
{
    uint8_t charge, input, precharge, termination, voltage;
    uint16_t vindpm;
    shp8808_status result;

    if (device == 0 || config == 0 || config->fb_voltage_mv < 1504U || config->fb_voltage_mv > 1566U ||
        ((config->fb_voltage_mv - 1504U) & 1U) != 0U || config->vindpm_ref_mv < 100U || config->vindpm_ref_mv > 1123U)
        return SHP8808_EINVAL;

    voltage = (uint8_t)((config->fb_voltage_mv - 1504U) / 2U);
    vindpm = (uint16_t)(config->vindpm_ref_mv - 100U);
    result = encode_current(config->charge_current_ma, battery_sense(device), SHP8808_ICHG_OFFSET_MA,
                            SHP8808_ICHG_STEP_MA, SHP8808_IBAT_REF_MOHM, 0xBEU, SHP8808_ICHG_MAX_MA, &charge);
    if (result != SHP8808_OK)
        return result;
    result = encode_current(config->input_current_ma, input_sense(device), SHP8808_IBUS_OFFSET_MA, SHP8808_IBUS_STEP_MA,
                            SHP8808_IBUS_REF_MOHM, 0x7FU, SHP8808_IBUS_MAX_MA, &input);
    if (result != SHP8808_OK)
        return result;
    result = encode_current(config->precharge_current_ma, battery_sense(device), SHP8808_IPRE_OFFSET_MA,
                            SHP8808_IPRE_STEP_MA, SHP8808_IBAT_REF_MOHM, 0x60U, SHP8808_IPRE_MAX_MA, &precharge);
    if (result != SHP8808_OK)
        return result;
    result = encode_current(config->termination_current_ma, battery_sense(device), SHP8808_ITERM_OFFSET_MA,
                            SHP8808_ITERM_STEP_MA, SHP8808_IBAT_REF_MOHM, 0x3FU, SHP8808_ITERM_MAX_MA, &termination);
    if (result != SHP8808_OK)
        return result;

    if (shp8808_write8(device, SHP8808_REG_CHARGE_CURRENT, charge) != SHP8808_OK ||
        shp8808_write8(device, SHP8808_REG_PRECHARGE_CURRENT, precharge & 0x7FU) != SHP8808_OK ||
        shp8808_write8(device, SHP8808_REG_TERMINATION_CURRENT, termination & 0x3FU) != SHP8808_OK ||
        shp8808_write8(device, SHP8808_REG_CHARGE_VOLTAGE, voltage & 0x1FU) != SHP8808_OK ||
        shp8808_write8(device, SHP8808_REG_IINDPM_CURRENT, input & 0x7FU) != SHP8808_OK ||
        shp8808_write16(device, SHP8808_REG_VINDPM_VOLTAGE, vindpm & 0x07FFU) != SHP8808_OK)
        return SHP8808_EIO;

    result = update8(device, SHP8808_REG_CHARGE_OPTION1, 0x1CU,
                     (uint8_t)((config->enable_termination ? 0x10U : 0U) | (config->enable_precharge ? 0x08U : 0U) |
                               (config->enable_float ? 0x04U : 0U)));
    if (result != SHP8808_OK)
        return result;
    return update8(device, SHP8808_REG_CHARGE_OPTION1, 0xC0U, 0U);
}

shp8808_status shp8808_set_dcdc_enabled(const shp8808 *device, bool enabled)
{
    return update8(device, SHP8808_REG_CHARGE_OPTION0, 0x02U, enabled ? 0U : 0x02U);
}

shp8808_status shp8808_set_charge_enabled(const shp8808 *device, bool enabled)
{
    shp8808_status result;

    if (enabled) {
        result = update8(device, SHP8808_REG_CHARGE_OPTION4, 0x80U, 0U);
        if (result != SHP8808_OK)
            return result;
        return shp8808_set_dcdc_enabled(device, true);
    }
    return shp8808_set_dcdc_enabled(device, false);
}

shp8808_status shp8808_disable_charging(const shp8808 *device)
{
    shp8808_status result;

    result = shp8808_set_reverse_enabled(device, false);
    if (result != SHP8808_OK)
        return result;
    result = shp8808_set_hiz(device, false);
    if (result != SHP8808_OK)
        return result;
    return shp8808_set_charge_enabled(device, false);
}

shp8808_status shp8808_set_hiz(const shp8808 *device, bool enabled)
{
    return shp8808_set_dcdc_enabled(device, !enabled);
}

shp8808_status shp8808_set_vbat_lowv(const shp8808 *device, shp8808_vbat_lowv threshold)
{
    if ((uint32_t)threshold > (uint32_t)SHP8808_VBAT_LOWV_714)
        return SHP8808_EINVAL;
    return update8(device, SHP8808_REG_CHARGE_OPTION0, 0xC0U, (uint8_t)((uint8_t)threshold << 6));
}

shp8808_status shp8808_set_recharge_threshold(const shp8808 *device, shp8808_vrechg threshold)
{
    if ((uint32_t)threshold > (uint32_t)SHP8808_VRECHG_975)
        return SHP8808_EINVAL;
    return update8(device, SHP8808_REG_CHARGE_OPTION0, 0x30U, (uint8_t)((uint8_t)threshold << 4));
}

shp8808_status shp8808_set_float_threshold(const shp8808 *device, shp8808_vfloat threshold)
{
    if ((uint32_t)threshold > (uint32_t)SHP8808_VFLOAT_95)
        return SHP8808_EINVAL;
    return update8(device, SHP8808_REG_CHARGE_OPTION0, 0x0CU, (uint8_t)((uint8_t)threshold << 2));
}

shp8808_status shp8808_set_safety_timer(const shp8808 *device, shp8808_safety_timer timer, bool enabled)
{
    if ((uint32_t)timer > (uint32_t)SHP8808_SAFETY_TIMER_24H)
        return SHP8808_EINVAL;
    return update8(device, SHP8808_REG_CHARGE_OPTION3, 0x0BU,
                   (uint8_t)((enabled ? 0x08U : 0U) | (uint8_t)timer));
}

shp8808_status shp8808_set_precharge_timer(const shp8808 *device, shp8808_precharge_timer timer, bool enabled)
{
    if ((uint32_t)timer > (uint32_t)SHP8808_PRECHARGE_TIMER_1H)
        return SHP8808_EINVAL;
    return update8(device, SHP8808_REG_CHARGE_OPTION3, 0x14U,
                   (uint8_t)((enabled ? 0x10U : 0U) | ((uint8_t)timer << 2)));
}

shp8808_status shp8808_set_inductor_ocp(const shp8808 *device, shp8808_il_avg limit)
{
    uint8_t code;

    if ((uint32_t)limit > (uint32_t)SHP8808_IL_AVG_40A)
        return SHP8808_EINVAL;
    code = (limit == SHP8808_IL_AVG_15A) ? 0x01U : ((limit == SHP8808_IL_AVG_40A) ? 0x02U : 0x00U);
    return update8(device, SHP8808_REG_CHARGE_OPTION2, 0x03U, code);
}

shp8808_status shp8808_configure_reverse(const shp8808 *device, const shp8808_reverse_config *config)
{
    uint16_t voltage;
    uint8_t current;
    shp8808_status result;

    if (device == 0 || config == 0)
        return SHP8808_EINVAL;
    result = encode_otg_voltage(config->vbus_mv, &voltage);
    if (result != SHP8808_OK)
        return result;
    result = encode_current(config->ibus_ma, input_sense(device), SHP8808_IBUS_OFFSET_MA, SHP8808_IBUS_STEP_MA,
                            SHP8808_IBUS_REF_MOHM, 0x7FU, SHP8808_IBUS_MAX_MA, &current);
    if (result != SHP8808_OK)
        return result;
    if (shp8808_write16(device, SHP8808_REG_OTG_VOLTAGE, voltage & 0x07FFU) != SHP8808_OK ||
        shp8808_write8(device, SHP8808_REG_OTG_CURRENT, current & 0x7FU) != SHP8808_OK)
        return SHP8808_EIO;
    return update8(device, SHP8808_REG_CHARGE_OPTION4, 0x40U, config->fb_pin_enabled ? 0x40U : 0U);
}

shp8808_status shp8808_set_reverse_enabled(const shp8808 *device, bool enabled)
{
    shp8808_status result;

    if (enabled) {
        result = shp8808_set_dcdc_enabled(device, true);
        if (result != SHP8808_OK)
            return result;
        return update8(device, SHP8808_REG_CHARGE_OPTION4, 0x80U, 0x80U);
    }
    return update8(device, SHP8808_REG_CHARGE_OPTION4, 0x80U, 0U);
}

shp8808_status shp8808_set_otg_voltage_offset(const shp8808 *device, int16_t offset_mv)
{
    uint8_t code;

    if ((offset_mv % 50) != 0 || offset_mv < -350 || offset_mv > 400)
        return SHP8808_EINVAL;
    if (offset_mv >= 0)
        code = (uint8_t)(offset_mv / 50);
    else
        code = (uint8_t)(0x08U + ((-offset_mv) / 50));
    return update8(device, SHP8808_REG_OTG_VOLTAGE_OFFSET, 0x0FU, code);
}

shp8808_status shp8808_configure_mppt(const shp8808 *device, const shp8808_mppt_config *config)
{
    if (device == 0 || config == 0 || (uint32_t)config->perturb > (uint32_t)SHP8808_MPPT_PO_OFF ||
        (uint32_t)config->full_sweep > (uint32_t)SHP8808_MPPT_SWEEP_OFF ||
        (uint32_t)config->step > (uint32_t)SHP8808_MPPT_STEP_8X)
        return SHP8808_EINVAL;
    return update8(device, SHP8808_REG_MPPT_CONTROL0, 0x3FU,
                   (uint8_t)(((uint8_t)config->full_sweep << 4) | ((uint8_t)config->perturb << 2) |
                             (uint8_t)config->step));
}

shp8808_status shp8808_set_mppt_enabled(const shp8808 *device, bool enabled)
{
    return update8(device, SHP8808_REG_MPPT_CONTROL0, 0x80U, enabled ? 0x80U : 0U);
}

shp8808_status shp8808_set_ts_enabled(const shp8808 *device, bool enabled)
{
    return update8(device, SHP8808_REG_JEITA_CONTROL1, 0x01U, enabled ? 0U : 0x01U);
}

shp8808_status shp8808_set_jeita_enabled(const shp8808 *device, bool enabled)
{
    return update8(device, SHP8808_REG_JEITA_CONTROL0, 0x01U, enabled ? 0x01U : 0U);
}

shp8808_status shp8808_set_pfm(const shp8808 *device, bool enabled)
{
    return update8(device, SHP8808_REG_CHARGE_OPTION4, 0x03U, enabled ? 0x03U : 0U);
}

shp8808_status shp8808_set_vbus_pulldown(const shp8808 *device, bool enabled)
{
    return update8(device, SHP8808_REG_CHARGE_OPTION5, 0x08U, enabled ? 0x08U : 0U);
}

shp8808_status shp8808_read_status(const shp8808 *device, shp8808_status_snapshot *status)
{
    uint8_t reg = SHP8808_REG_STATUS0;

    if (device == 0 || status == 0)
        return SHP8808_EINVAL;
    (void)device->i2c_inst;
    return board_i2c_write_read(device->address, &reg, 1U, (uint8_t *)status, sizeof(*status), 50U) == 0 ? SHP8808_OK
                                                                                                        : SHP8808_EIO;
}

void shp8808_decode_status(const shp8808_status_snapshot *raw, shp8808_status_decoded *decoded)
{
    uint8_t phase;

    if (raw == 0 || decoded == 0)
        return;

    decoded->ibus_reg = (raw->status0 & 0x80U) != 0U;
    decoded->vindpm = (raw->status0 & 0x40U) != 0U;
    decoded->pg = (raw->status0 & 0x20U) != 0U;
    decoded->vbus_present = (raw->status0 & 0x10U) != 0U;
    decoded->vbus_ov = (raw->status0 & 0x08U) != 0U;
    decoded->vbat_ov = (raw->status0 & 0x04U) != 0U;
    decoded->il_clamp = (raw->status0 & 0x02U) != 0U;

    phase = (uint8_t)((raw->status1 >> 5) & 0x07U);
    decoded->charge = (shp8808_charge_phase)phase;
    decoded->fast_timer = (raw->status1 & 0x10U) != 0U;
    decoded->precharge_timer = (raw->status1 & 0x08U) != 0U;
    decoded->trickle_timer = (raw->status1 & 0x04U) != 0U;
    decoded->otg_ov = (raw->status1 & 0x02U) != 0U;
    decoded->otg_uv = (raw->status1 & 0x01U) != 0U;
    decoded->safety_timer = decoded->fast_timer || decoded->precharge_timer || decoded->trickle_timer;

    if ((raw->status2 & 0x10U) != 0U)
        decoded->ts = SHP8808_TS_HOT;
    else if ((raw->status2 & 0x80U) != 0U)
        decoded->ts = SHP8808_TS_COLD;
    else if ((raw->status2 & 0x20U) != 0U)
        decoded->ts = SHP8808_TS_WARM;
    else if ((raw->status2 & 0x40U) != 0U)
        decoded->ts = SHP8808_TS_COOL;
    else
        decoded->ts = SHP8808_TS_NORMAL;

    decoded->mppt = (shp8808_mppt_state)((raw->status2 >> 2) & 0x03U);
    decoded->reverse = (raw->status2 & 0x02U) != 0U;
    decoded->sync = (raw->status2 & 0x01U) != 0U;
}

shp8808_status shp8808_read_and_clear_flags(const shp8808 *device, shp8808_flags *flags)
{
    shp8808_status_snapshot status;
    shp8808_status result;

    result = shp8808_read_status(device, &status);
    if (result != SHP8808_OK)
        return result;
    flags->status0 = status.status0;
    flags->status1 = status.status1;
    flags->status2 = status.status2;
    /* OTG OVP/UVP are read-cleared; write 0 to drop sticky bits. */
    return update8(device, SHP8808_REG_STATUS1, 0x03U, 0U);
}

shp8808_status shp8808_adc_configure(const shp8808 *device, uint8_t channels, bool continuous)
{
    uint8_t value;

    if (device == 0 || (channels & (uint8_t)~SHP8808_ADC_ALL) != 0U || channels == 0U)
        return SHP8808_EINVAL;
    value = (uint8_t)(0x80U | (continuous ? 0U : 0x40U) | channels);
    return shp8808_write8(device, SHP8808_REG_ADC_CONTROL, value);
}

shp8808_status shp8808_adc_start(const shp8808 *device)
{
    return update8(device, SHP8808_REG_ADC_CONTROL, 0xA0U, 0xA0U);
}

shp8808_status shp8808_adc_wait_done(const shp8808 *device, uint32_t attempts)
{
    uint8_t control;
    uint32_t i;

    if (device == 0 || attempts == 0U)
        return SHP8808_EINVAL;
    for (i = 0U; i < attempts; i++) {
        if (shp8808_read8(device, SHP8808_REG_ADC_CONTROL, &control) != SHP8808_OK)
            return SHP8808_EIO;
        if ((control & 0x40U) == 0U)
            return SHP8808_OK;
        if ((control & 0x20U) == 0U)
            return SHP8808_OK;
    }
    return SHP8808_ETIMEOUT;
}

shp8808_status shp8808_adc_read(const shp8808 *device, shp8808_adc_values *values)
{
    uint16_t raw_ibus, raw_iotg, raw_ichg, raw_idchg, raw_vbus, raw_vbat, raw_ts;
    uint16_t isense, bsense;
    uint32_t ichg, idchg;

    if (device == 0 || values == 0)
        return SHP8808_EINVAL;
    if (shp8808_read16(device, SHP8808_REG_ADC_IBUS, &raw_ibus) != SHP8808_OK ||
        shp8808_read16(device, SHP8808_REG_ADC_IOTG, &raw_iotg) != SHP8808_OK ||
        shp8808_read16(device, SHP8808_REG_ADC_ICHG, &raw_ichg) != SHP8808_OK ||
        shp8808_read16(device, SHP8808_REG_ADC_IDCHG, &raw_idchg) != SHP8808_OK ||
        shp8808_read16(device, SHP8808_REG_ADC_VBUS, &raw_vbus) != SHP8808_OK ||
        shp8808_read16(device, SHP8808_REG_ADC_VBAT, &raw_vbat) != SHP8808_OK ||
        shp8808_read16(device, SHP8808_REG_ADC_VNTC, &raw_ts) != SHP8808_OK)
        return SHP8808_EIO;

    isense = input_sense(device);
    bsense = battery_sense(device);
    ichg = decode_adc_current(raw_ichg, bsense, SHP8808_IBAT_REF_MOHM, SHP8808_ADC_IBAT_LSB_MA);
    idchg = decode_adc_current(raw_idchg, bsense, SHP8808_IBAT_REF_MOHM, SHP8808_ADC_IBAT_LSB_MA);
    values->input_current_ma =
        (int32_t)decode_adc_current(raw_ibus, isense, SHP8808_IBUS_REF_MOHM, SHP8808_ADC_IBUS_LSB_MA);
    values->otg_current_ma =
        (int32_t)decode_adc_current(raw_iotg, isense, SHP8808_IBUS_REF_MOHM, SHP8808_ADC_IBUS_LSB_MA);
    values->battery_current_ma = (int32_t)ichg - (int32_t)idchg;
    values->input_voltage_mv = ((uint32_t)raw_vbus & 0x07FFU) * 40U;
    values->battery_voltage_mv = ((uint32_t)raw_vbat & 0x07FFU) * 40U;
    values->ts_permille = (uint16_t)(((uint32_t)raw_ts & 0x07FFU) / 2U);
    return SHP8808_OK;
}

const char *shp8808_charge_phase_name(shp8808_charge_phase phase)
{
    static const char *const names[] = {"idle", "trickle", "precharge", "cc", "cv", "done", "float", "reserved"};

    if ((uint32_t)phase >= (uint32_t)(sizeof(names) / sizeof(names[0])))
        return "unknown";
    return names[phase];
}

const char *shp8808_ts_state_name(shp8808_ts_state state)
{
    static const char *const names[] = {"normal", "warm", "cool", "cold", "hot"};

    if ((uint32_t)state >= (uint32_t)(sizeof(names) / sizeof(names[0])))
        return "unknown";
    return names[state];
}

const char *shp8808_mppt_state_name(shp8808_mppt_state state)
{
    static const char *const names[] = {"off", "idle", "sweep", "locked"};

    if ((uint32_t)state >= (uint32_t)(sizeof(names) / sizeof(names[0])))
        return "unknown";
    return names[state];
}
