#ifndef MODULES_SHP8808_H
#define MODULES_SHP8808_H

#include <stdbool.h>
#include <stdint.h>

#define SHP8808_I2C_ADDRESS 0x6CU

#define SHP8808_REG_CHARGE_CURRENT 0x00U
#define SHP8808_REG_PRECHARGE_CURRENT 0x01U
#define SHP8808_REG_TERMINATION_CURRENT 0x02U
#define SHP8808_REG_CHARGE_VOLTAGE 0x03U
#define SHP8808_REG_OTG_VOLTAGE 0x04U
#define SHP8808_REG_OTG_CURRENT 0x06U
#define SHP8808_REG_IINDPM_CURRENT 0x07U
#define SHP8808_REG_VINDPM_VOLTAGE 0x08U
#define SHP8808_REG_OTG_VOLTAGE_OFFSET 0x0FU
#define SHP8808_REG_CHARGE_OPTION0 0x10U
#define SHP8808_REG_CHARGE_OPTION1 0x11U
#define SHP8808_REG_CHARGE_OPTION2 0x12U
#define SHP8808_REG_MPPT_CONTROL0 0x13U
#define SHP8808_REG_MPPT_CONTROL1 0x14U
#define SHP8808_REG_CHARGE_OPTION3 0x15U
#define SHP8808_REG_CHARGE_OPTION4 0x16U
#define SHP8808_REG_CHARGE_OPTION5 0x17U
#define SHP8808_REG_JEITA_CONTROL0 0x18U
#define SHP8808_REG_JEITA_CONTROL1 0x19U
#define SHP8808_REG_ADC_CONTROL 0x1AU
#define SHP8808_REG_STATUS0 0x20U
#define SHP8808_REG_STATUS1 0x21U
#define SHP8808_REG_STATUS2 0x22U
#define SHP8808_REG_ADC_IBUS 0x30U
#define SHP8808_REG_ADC_IOTG 0x32U
#define SHP8808_REG_ADC_ICHG 0x34U
#define SHP8808_REG_ADC_IDCHG 0x36U
#define SHP8808_REG_ADC_VBUS 0x38U
#define SHP8808_REG_ADC_VBAT 0x3AU
#define SHP8808_REG_ADC_VNTC 0x3CU
#define SHP8808_REG_MPPT_CONTROL2 0x40U

#define SHP8808_ADC_IBUS (1U << 4)
#define SHP8808_ADC_IBAT (1U << 3)
#define SHP8808_ADC_VBUS (1U << 2)
#define SHP8808_ADC_VBAT (1U << 1)
#define SHP8808_ADC_TS (1U << 0)
#define SHP8808_ADC_ALL 0x1FU
#define SHP8808_ADC_MONITOR (SHP8808_ADC_IBUS | SHP8808_ADC_IBAT | SHP8808_ADC_VBUS | SHP8808_ADC_VBAT)

typedef enum {
    SHP8808_OK = 0,
    SHP8808_EINVAL = -1,
    SHP8808_EIO = -2,
    SHP8808_ENODEV = -3,
    SHP8808_ETIMEOUT = -4
} shp8808_status;

typedef enum {
    SHP8808_SAFETY_TIMER_5H = 0,
    SHP8808_SAFETY_TIMER_8H,
    SHP8808_SAFETY_TIMER_12H,
    SHP8808_SAFETY_TIMER_24H
} shp8808_safety_timer;

typedef enum { SHP8808_PRECHARGE_TIMER_2H = 0, SHP8808_PRECHARGE_TIMER_1H } shp8808_precharge_timer;

typedef enum {
    SHP8808_VBAT_LOWV_30 = 0,
    SHP8808_VBAT_LOWV_55,
    SHP8808_VBAT_LOWV_667,
    SHP8808_VBAT_LOWV_714
} shp8808_vbat_lowv;

typedef enum { SHP8808_VRECHG_93 = 0, SHP8808_VRECHG_945, SHP8808_VRECHG_95, SHP8808_VRECHG_975 } shp8808_vrechg;

typedef enum { SHP8808_VFLOAT_905 = 0, SHP8808_VFLOAT_93, SHP8808_VFLOAT_945, SHP8808_VFLOAT_95 } shp8808_vfloat;

typedef enum {
    SHP8808_CHARGE_NOT_CHARGING = 0,
    SHP8808_CHARGE_TRICKLE,
    SHP8808_CHARGE_PRECHARGE,
    SHP8808_CHARGE_FAST_CC,
    SHP8808_CHARGE_TAPER_CV,
    SHP8808_CHARGE_DONE,
    SHP8808_CHARGE_FLOAT,
    SHP8808_CHARGE_RESERVED
} shp8808_charge_phase;

typedef enum {
    SHP8808_TS_NORMAL = 0,
    SHP8808_TS_WARM,
    SHP8808_TS_COOL,
    SHP8808_TS_COLD,
    SHP8808_TS_HOT
} shp8808_ts_state;

typedef enum {
    SHP8808_MPPT_DISABLED = 0,
    SHP8808_MPPT_IDLE,
    SHP8808_MPPT_SWEEP,
    SHP8808_MPPT_LOCKED
} shp8808_mppt_state;

typedef enum {
    SHP8808_MPPT_PO_62MS = 0,
    SHP8808_MPPT_PO_125MS,
    SHP8808_MPPT_PO_1250MS,
    SHP8808_MPPT_PO_OFF
} shp8808_mppt_po_timer;

typedef enum {
    SHP8808_MPPT_SWEEP_5MIN = 0,
    SHP8808_MPPT_SWEEP_10MIN,
    SHP8808_MPPT_SWEEP_15MIN,
    SHP8808_MPPT_SWEEP_OFF
} shp8808_mppt_sweep_timer;

typedef enum {
    SHP8808_MPPT_STEP_1X = 0,
    SHP8808_MPPT_STEP_2X,
    SHP8808_MPPT_STEP_4X,
    SHP8808_MPPT_STEP_8X
} shp8808_mppt_step;

typedef enum {
    SHP8808_IL_AVG_25A = 0,
    SHP8808_IL_AVG_15A,
    SHP8808_IL_AVG_40A
} shp8808_il_avg;

typedef struct {
    uint8_t i2c_inst;
    uint8_t address;
    uint16_t input_sense_mohm;
    uint16_t battery_sense_mohm;
    uint32_t timeout_cycles;
} shp8808;

typedef struct {
    uint16_t fb_voltage_mv;
    uint32_t charge_current_ma;
    uint32_t input_current_ma;
    uint16_t vindpm_ref_mv;
    uint32_t precharge_current_ma;
    uint32_t termination_current_ma;
    bool enable_precharge;
    bool enable_termination;
    bool enable_float;
} shp8808_charge_config;

typedef struct {
    bool ibus_pin_enabled;
    bool ibat_pin_enabled;
    bool otg_fb_pin_enabled;
} shp8808_pin_config;

typedef struct {
    uint32_t vbus_mv;
    uint32_t ibus_ma;
    bool fb_pin_enabled;
} shp8808_reverse_config;

typedef struct {
    shp8808_mppt_po_timer perturb;
    shp8808_mppt_sweep_timer full_sweep;
    shp8808_mppt_step step;
} shp8808_mppt_config;

typedef struct {
    uint8_t status0;
    uint8_t status1;
    uint8_t status2;
} shp8808_status_snapshot;

typedef struct {
    shp8808_charge_phase charge;
    shp8808_ts_state ts;
    shp8808_mppt_state mppt;
    bool ibus_reg;
    bool vindpm;
    bool pg;
    bool vbus_present;
    bool vbus_ov;
    bool vbat_ov;
    bool il_clamp;
    bool reverse;
    bool sync;
    bool otg_ov;
    bool otg_uv;
    bool fast_timer;
    bool precharge_timer;
    bool trickle_timer;
    bool safety_timer;
} shp8808_status_decoded;

typedef struct {
    uint8_t status0;
    uint8_t status1;
    uint8_t status2;
} shp8808_flags;

typedef struct {
    int32_t input_current_ma;
    int32_t otg_current_ma;
    int32_t battery_current_ma;
    uint32_t input_voltage_mv;
    uint32_t battery_voltage_mv;
    uint16_t ts_permille;
} shp8808_adc_values;

/* Bus */
shp8808_status shp8808_read8(const shp8808 *device, uint8_t reg, uint8_t *value);
shp8808_status shp8808_write8(const shp8808 *device, uint8_t reg, uint8_t value);
shp8808_status shp8808_read16(const shp8808 *device, uint8_t reg, uint16_t *value);
shp8808_status shp8808_write16(const shp8808 *device, uint8_t reg, uint16_t value);

/* Device */
shp8808_status shp8808_probe(const shp8808 *device, uint8_t *option0);
shp8808_status shp8808_reset_registers(const shp8808 *device);

/* Pins */
shp8808_status shp8808_configure_pins(const shp8808 *device, const shp8808_pin_config *config);
shp8808_status shp8808_configure_software_host(const shp8808 *device);

/* Charge */
shp8808_status shp8808_configure_charging(const shp8808 *device, const shp8808_charge_config *config);
shp8808_status shp8808_set_charge_enabled(const shp8808 *device, bool enabled);
shp8808_status shp8808_disable_charging(const shp8808 *device);
shp8808_status shp8808_set_dcdc_enabled(const shp8808 *device, bool enabled);
shp8808_status shp8808_set_hiz(const shp8808 *device, bool enabled);
shp8808_status shp8808_set_vbat_lowv(const shp8808 *device, shp8808_vbat_lowv threshold);
shp8808_status shp8808_set_recharge_threshold(const shp8808 *device, shp8808_vrechg threshold);
shp8808_status shp8808_set_float_threshold(const shp8808 *device, shp8808_vfloat threshold);
shp8808_status shp8808_set_safety_timer(const shp8808 *device, shp8808_safety_timer timer, bool enabled);
shp8808_status shp8808_set_precharge_timer(const shp8808 *device, shp8808_precharge_timer timer, bool enabled);
shp8808_status shp8808_set_inductor_ocp(const shp8808 *device, shp8808_il_avg limit);

/* Reverse / OTG */
shp8808_status shp8808_configure_reverse(const shp8808 *device, const shp8808_reverse_config *config);
shp8808_status shp8808_set_reverse_enabled(const shp8808 *device, bool enabled);
shp8808_status shp8808_set_otg_voltage_offset(const shp8808 *device, int16_t offset_mv);

/* MPPT */
shp8808_status shp8808_configure_mppt(const shp8808 *device, const shp8808_mppt_config *config);
shp8808_status shp8808_set_mppt_enabled(const shp8808 *device, bool enabled);

/* TS */
shp8808_status shp8808_set_ts_enabled(const shp8808 *device, bool enabled);
shp8808_status shp8808_set_jeita_enabled(const shp8808 *device, bool enabled);

/* Converter */
shp8808_status shp8808_set_pfm(const shp8808 *device, bool enabled);
shp8808_status shp8808_set_vbus_pulldown(const shp8808 *device, bool enabled);

/* Status */
shp8808_status shp8808_read_status(const shp8808 *device, shp8808_status_snapshot *status);
void shp8808_decode_status(const shp8808_status_snapshot *raw, shp8808_status_decoded *decoded);
shp8808_status shp8808_read_and_clear_flags(const shp8808 *device, shp8808_flags *flags);

/* ADC */
shp8808_status shp8808_adc_configure(const shp8808 *device, uint8_t channels, bool continuous);
shp8808_status shp8808_adc_start(const shp8808 *device);
shp8808_status shp8808_adc_wait_done(const shp8808 *device, uint32_t attempts);
shp8808_status shp8808_adc_read(const shp8808 *device, shp8808_adc_values *values);

const char *shp8808_charge_phase_name(shp8808_charge_phase phase);
const char *shp8808_ts_state_name(shp8808_ts_state state);
const char *shp8808_mppt_state_name(shp8808_mppt_state state);

#endif
