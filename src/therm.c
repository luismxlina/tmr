#include "therm.h"

#include <math.h>

esp_err_t therm_config(therm_t* thermistor, adc_oneshot_unit_handle_t adc, adc_channel_t channel, float series_resistance, float nominal_resistance, float nominal_temperature, float beta_coefficient) {
    thermistor->adc_hdlr = adc;
    thermistor->adc_channel = channel;
    thermistor->series_resistance = series_resistance;
    thermistor->nominal_resistance = nominal_resistance;
    thermistor->nominal_temperature = nominal_temperature;
    thermistor->beta_coefficient = beta_coefficient;
    return ESP_OK;
}

float therm_read_t(therm_t thermistor) {
    uint16_t lsb = therm_read_lsb(thermistor);
    float voltage = _therm_lsb2v(lsb);
    return _therm_v2t(voltage, thermistor.series_resistance, thermistor.nominal_resistance, thermistor.nominal_temperature, thermistor.beta_coefficient);
}

float therm_read_v(therm_t thermistor) {
    uint16_t lsb = therm_read_lsb(thermistor);
    return _therm_lsb2v(lsb);
}

uint16_t therm_read_lsb(therm_t thermistor) {
    int raw_value = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(thermistor.adc_hdlr, thermistor.adc_channel, &raw_value));
    return raw_value;
}

float _therm_v2t(float v, float series_resistance, float nominal_resistance, float nominal_temperature, float beta_coefficient) {
    float r_ntc = series_resistance * (3.3 - v) / v;
    float t_kelvin = 1.0f / (1.0f / nominal_temperature + (1.0f / beta_coefficient) * log(r_ntc / nominal_resistance));
    return t_kelvin - 273.15f;
}

float _therm_lsb2v(uint16_t lsb) {
    return (float)(lsb * 3.3f / 4095.0f);
}