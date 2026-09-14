#ifndef MODULES_POWER_H
#define MODULES_POWER_H

#include <stdbool.h>
#include <stdint.h>
#include "modules/power/bq25756/bq25756.h"

#define POWER_FAULT_LOG_MAX 16U

typedef struct {
    uint32_t t_ms;
    uint8_t kind;
    uint8_t bits;
} power_fault_event;

typedef struct {
    bool present;
    uint8_t part;
    uint32_t uptime_ms;
    bq25756_adc_values adc;
    bq25756_status_snapshot raw;
    bq25756_status_decoded decoded;
    bool charge_en;
    bool hiz;
    bool reverse_en;
    bool mppt_en;
    bool pfm;
    bool ts_en;
    bool jeita_en;
    uint8_t watchdog;
    uint8_t safety_timer;
    bool safety_en;
    uint8_t topoff;
    uint8_t vbat_lowv;
    uint8_t vrechg;
    bq25756_charge_config charge;
    bq25756_reverse_config reverse;
    bq25756_mppt_config mppt;
    bq25756_gate_drive_config gate;
    bq25756_pin_config pins;
    uint8_t latch_fault;
    uint8_t latch_flag1;
    uint8_t latch_flag2;
    uint8_t latch_fault_flag;
    uint8_t log_count;
    power_fault_event log[POWER_FAULT_LOG_MAX];
} power_bq_snapshot;

int power_module_start(void);
int power_bq_copy_snapshot(power_bq_snapshot *out);
int power_bq_command(const char *json, uint32_t json_len);

#endif
