#ifndef __THERM_H__
#define __THERM_H__

#include <esp_adc/adc_oneshot.h>
#include <hal/adc_types.h>

typedef struct {
    adc_oneshot_unit_handle_t adc_hdlr;
    adc_channel_t adc_channel;
    float series_resistance;
    float nominal_resistance;
    float nominal_temperature;
    float beta_coefficient;
} therm_t;

esp_err_t therm_config(therm_t* thermistor, adc_oneshot_unit_handle_t adc, adc_channel_t channel, float series_resistance, float nominal_resistance, float nominal_temperature, float beta_coefficient);
float therm_read_t(therm_t thermistor);
float therm_read_v(therm_t thermistor);
uint16_t therm_read_lsb(therm_t thermistor);

float _therm_v2t(float v, float series_resistance, float nominal_resistance, float nominal_temperature, float beta_coefficient);
float _therm_lsb2v(uint16_t lsb);

#endif