#include "therm.h"

#include <driver/adc.h>
#include <driver/gpio.h>
#include <math.h>

#include "config.h"

// Manejador ADC estático compartido por todos los termistores
static adc_oneshot_unit_handle_t shared_adc_hdlr = NULL;
static bool adc_initialized = false;

esp_err_t therm_init(therm_t* thermistor, adc_channel_t channel, gpio_num_t power_gpio,
                     float series_resistance, float nominal_resistance,
                     float nominal_temperature, float beta_coefficient) {
    // Inicializa el ADC solo una vez
    if (!adc_initialized) {
        adc_oneshot_unit_init_cfg_t unit_cfg = {
            .unit_id = THERMISTOR_ADC_UNIT,
            .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        };
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &shared_adc_hdlr));
        adc_initialized = true;
    }
    // Configura el GPIO para el control de energía
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << power_gpio),
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    // Configura el termistor
    thermistor->adc_hdlr = shared_adc_hdlr;
    thermistor->adc_channel = channel;
    thermistor->power_gpio = power_gpio;
    thermistor->series_resistance = series_resistance;
    thermistor->nominal_resistance = nominal_resistance;
    thermistor->nominal_temperature = nominal_temperature;
    thermistor->beta_coefficient = beta_coefficient;

    // Configura el canal ADC
    adc_oneshot_chan_cfg_t channel_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(shared_adc_hdlr, channel, &channel_cfg));

    return ESP_OK;
}

// Lee la temperatura del termistor
float therm_read_temperature(therm_t thermistor) {
    uint16_t lsb = therm_read_lsb(thermistor);
    float voltage = _therm_lsb_to_voltage(lsb);
    return _therm_voltage_to_temperature(voltage, thermistor.series_resistance, thermistor.nominal_resistance, thermistor.nominal_temperature, thermistor.beta_coefficient);
}

// Lee el voltaje del termistor
float therm_read_voltage(therm_t thermistor) {
    uint16_t lsb = therm_read_lsb(thermistor);
    return _therm_lsb_to_voltage(lsb);
}

// Lee el valor LSB del termistor
uint16_t therm_read_lsb(therm_t thermistor) {
    int raw_value = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(thermistor.adc_hdlr, thermistor.adc_channel, &raw_value));
    return raw_value;
}

void therm_power_on(therm_t thermistor) {
    gpio_set_level(thermistor.power_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(10));  // Permite tiempo de estabilización
}

void therm_power_off(therm_t thermistor) {
    gpio_set_level(thermistor.power_gpio, 0);
}

// Convierte el voltaje a temperatura en grados Celsius
float _therm_voltage_to_temperature(float voltage, float series_resistance, float nominal_resistance, float nominal_temperature, float beta_coefficient) {
    float r_ntc = series_resistance * (3.3 - voltage) / voltage;
    float t_kelvin = 1.0f / (1.0f / nominal_temperature + (1.0f / beta_coefficient) * log(r_ntc / nominal_resistance));
    return t_kelvin - 273.15f;
}

// Convierte el valor LSB a voltaje
float _therm_lsb_to_voltage(uint16_t lsb) {
    return (float)(lsb * 3.3f / 4095.0f);
}