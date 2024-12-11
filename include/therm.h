#ifndef __THERM_H__
#define __THERM_H__

#include <esp_adc/adc_oneshot.h>
#include <hal/adc_types.h>
#include <soc/gpio_num.h>

// Estructura para la configuración del termistor
typedef struct {
    adc_oneshot_unit_handle_t adc_hdlr;
    adc_channel_t adc_channel;
    gpio_num_t power_gpio;
    float series_resistance;
    float nominal_resistance;
    float nominal_temperature;
    float beta_coefficient;
} therm_t;

// Funciones públicas para la configuración y uso del termistor
esp_err_t therm_init(therm_t* thermistor, adc_channel_t channel, gpio_num_t power_gpio, float series_resistance, float nominal_resistance, float nominal_temperature, float beta_coefficient);
float therm_read_temperature(therm_t thermistor);
float therm_read_voltage(therm_t thermistor);
uint16_t therm_read_lsb(therm_t thermistor);
void therm_power_on(therm_t thermistor);
void therm_power_off(therm_t thermistor);

// Funciones útiles de conversión
float _therm_voltage_to_temperature(float voltage, float series_resistance, float nominal_resistance, float nominal_temperature, float beta_coefficient);
float _therm_lsb_to_voltage(uint16_t lsb);

#endif  // __THERM_H__