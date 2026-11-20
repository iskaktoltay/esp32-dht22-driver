#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/gpio_types.h"
#include "soc/gpio_num.h"
#include <driver/gpio.h>
#include <esp_rom_sys.h>
#include <inttypes.h>
#include <stdint.h>
#include "esp_check.h"
#include "esp_timer.h"

#define DHT_GPIO GPIO_NUM_4
static const char *TAG = "dht22";

// DATA: 40 bits = 16-bit humidity + 16-bit temperature + 8-bit checksum.
// Both values are sent in tenths (e.g. 652 = 65.2 %RH).
// The top bit of the temperature is a sign bit.
// Checksum equals low 8 bits of the sum of the first four bytes.

static esp_err_t wait_level_change(int64_t timeout_us, int curr_level, uint32_t* out_duration) {
  int64_t init_val = esp_timer_get_time();
  int64_t loop_val = init_val;
  while (gpio_get_level(DHT_GPIO) == curr_level && (loop_val - init_val) < timeout_us) {
    loop_val = esp_timer_get_time();
  }
  if ((loop_val - init_val) >= timeout_us) {return ESP_ERR_TIMEOUT;}
  if (out_duration != NULL) {
    *out_duration = loop_val - init_val;
  }
  return ESP_OK;
}

static esp_err_t acc_single_bit(uint8_t* data, uint32_t* out_duration) {
  uint32_t duration_us = 0;
  esp_err_t err = ESP_OK;
  // The timeout values are ceiling times 2
  // Wait for initial signal
  err = wait_level_change(100, 0, NULL);
  ESP_RETURN_ON_ERROR(err, TAG, "Returned err from get single bit func");
  // Get the bit data
  err = wait_level_change(140, 1, &duration_us);
  ESP_RETURN_ON_ERROR(err, TAG, "Returned err from get single bit func");
  *data = *data << 1;
  // 47 as middle ground between 28 and 80 us
  if (duration_us > 47) {
    *data |= 1U;
  }
  if (out_duration != NULL) {*out_duration = duration_us;}
  return ESP_OK;
}

esp_err_t read_data(int16_t* temp_tenths_c, uint16_t* humidity_tenths_c)
{
  esp_err_t err = ESP_OK;
  // Init signal to DHT22
  gpio_set_level(DHT_GPIO, 0);
  esp_rom_delay_us(1000);
  // Wait for sensor response
  gpio_set_level(DHT_GPIO, 1);
  err = wait_level_change(80, 1, NULL);
  if (err != ESP_OK) return err;
  // Wait for sensor preparation to send data
  err = wait_level_change(160, 0, NULL);
  if (err != ESP_OK) return err;
  err = wait_level_change(160, 1, NULL);
  if (err != ESP_OK) return err;
  uint8_t data[5] = {};
  uint32_t durations[40] = {};
  // Get readings from sensor
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 8; j++) {
      ESP_RETURN_ON_ERROR(acc_single_bit(&data[i], &durations[(i*8)+j]), TAG, "Returned in read loop");
    }
  }  
  // Collect sum for validation
  uint16_t sum = data[0] + data[1] + data[2] + data[3];
  uint8_t sum_masked = sum & 0xFF;

  if (sum_masked == data[4]) {
    uint8_t temp_sign_bit = data[2] >> 7;
    uint8_t temp_high_byte = data[2] & 0x7F;
    *temp_tenths_c = (temp_high_byte << 8) | data[3];
    if (temp_sign_bit == 1) *temp_tenths_c = -*temp_tenths_c;
    *humidity_tenths_c = (data[0] << 8) | data[1];
    return ESP_OK;
  }

  return ESP_ERR_INVALID_CRC;
}

void app_main(void)
{
  gpio_config_t dht_gpio_config = {
    .pin_bit_mask = (1ULL << DHT_GPIO),
    .mode = GPIO_MODE_INPUT_OUTPUT_OD,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
  };
  esp_err_t gpio_config_err = gpio_config(&dht_gpio_config);
  ESP_ERROR_CHECK(gpio_config_err);
  int16_t temp;
  uint16_t humidity;
  ESP_ERROR_CHECK(read_data(&temp, &humidity));
  ESP_LOGI(TAG, "Temp is: %" PRId16 "", temp);
  ESP_LOGI(TAG, "Humidity is: %" PRId16 "", humidity);
}
