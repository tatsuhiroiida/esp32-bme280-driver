/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <driver/i2c.h>
#include <esp_log.h>
#include <bme280.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

#define ACK_VAL 0x0  /*!< I2C ack value */
#define NACK_VAL 0x1 /*!< I2C nack value */

double centigrade_to_fahrenheit(uint32_t centigrade) {
	return ((double)centigrade)/100 * 9 / 5 + 32.0;
}

void print_sensor_data(struct bme280_data *comp_data)
{
  printf("temp %.02fF, p %zu, hum %zu\r\n", centigrade_to_fahrenheit(comp_data->temperature), comp_data->pressure, comp_data->humidity);
}

static uint8_t write_register(uint8_t id, uint8_t reg_addr, uint8_t *data, uint16_t len) {
  /* printf("----write_register()----\n"); */
  /* printf("\treg_addr: %#x\n", reg_addr); */
  /* printf("\tlen: %d\n", len); */
  /* printf("\tdata: %#x\n", *data); */

  i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  esp_err_t err = BME280_OK;
  i2c_master_start(cmd);
  //id equals BME280_I2C_ADDR_SEC (0x77)
  i2c_master_write_byte(cmd, (id << 1) | I2C_MASTER_WRITE, 1 /* expect ack */);
  i2c_master_write_byte(cmd, reg_addr, 1);
  i2c_master_write(cmd, data, len, 1);
  i2c_master_stop(cmd);

  esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
  i2c_cmd_link_delete(cmd);
  if (ret == ESP_OK) {
    //printf("Write OK\n");
  } else if (ret == ESP_ERR_TIMEOUT) {
      printf("Bus is busy\n");
  } else {
      printf("Write Failed\n");
  }
return 0;
}

static uint8_t read_register(uint8_t id, uint8_t reg_addr, uint8_t *data, uint16_t len) {
  //printf("----read_register()----\n");
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
  esp_err_t err = BME280_OK;
	i2c_master_start(cmd);
  //id equals BME280_I2C_ADDR_SEC (0x77)
	i2c_master_write_byte(cmd, (id << 1) | I2C_MASTER_WRITE, 1 /* expect ack */);
	i2c_master_write_byte(cmd, reg_addr, 1);
	i2c_master_stop(cmd);
	i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000/portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);

	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (id << 1) | I2C_MASTER_READ, 1 /* expect ack */);
  if (len > 1)
    err = i2c_master_read(cmd, data, len - 1, ACK_VAL);
  if (err != BME280_OK)
    return err;

  err = i2c_master_read_byte(cmd, data + len - 1, NACK_VAL);
	i2c_master_stop(cmd);
	i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000/portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);

  //printf("\tdata: %#x\n", *data);
  //printf("\terr: %#x\n", err);
  //printf("----end read_register()----\n");
	return err;
}

int8_t stream_sensor_data_forced_mode(struct bme280_dev *dev)
{
  int8_t rslt;
  uint8_t settings_sel;
  struct bme280_data comp_data;

  /* Recommended mode of operation: Indoor navigation */
  dev->settings.osr_h = BME280_OVERSAMPLING_1X;
  dev->settings.osr_p = BME280_OVERSAMPLING_16X;
  dev->settings.osr_t = BME280_OVERSAMPLING_2X;
  dev->settings.filter = BME280_FILTER_COEFF_16;

  settings_sel = BME280_OSR_PRESS_SEL | BME280_OSR_TEMP_SEL | BME280_OSR_HUM_SEL | BME280_FILTER_SEL;

  //TODO: Enable this
  rslt = bme280_set_sensor_settings(settings_sel, dev);

  //printf("Temperature, Pressure, Humidity\r\n");
  /* Continuously stream sensor data */
  while (1) {
    //TODO: Enable this
    rslt = bme280_set_sensor_mode(BME280_FORCED_MODE, dev);
    /* Wait for the measurement to complete and print data @25Hz */
    //dev->delay_ms(500);
    vTaskDelay(50/portTICK_PERIOD_MS);
    rslt = bme280_get_sensor_data(BME280_ALL, &comp_data, dev);
    print_sensor_data(&comp_data);
  }
  return rslt;
}

int8_t user_i2c_read(uint8_t id, uint8_t reg_addr, uint8_t *data, uint16_t len)
{
  //printf("----user_i2c_read()----\n");
  //printf("\treg_addr: %#x\n", reg_addr);
  //printf("\tlen: %d\n", len);
  if (read_register(id, reg_addr, data, len) != BME280_OK)
    return 1;
  //printf("\tdata: %#x\n", *data);
  //printf("----end user_i2c_read()----\n");
  return 0;
}

void user_delay_ms(uint32_t period)
{
  vTaskDelay((period*1000)/portTICK_PERIOD_MS);
}

int8_t user_i2c_write(uint8_t id, uint8_t reg_addr, uint8_t *data, uint16_t len)
{

  write_register(id, reg_addr, data, len);
  /* int8_t *buf; */
  /* buf = malloc(len +1); */
  /* buf[0] = reg_addr; */
  /* memcpy(buf +1, data, len); */
  /* write(fd, buf, len +1); */
  /* free(buf); */
  return 0;
}

void task_bme280(void *ignore) {
  struct bme280_dev dev;

	i2c_config_t conf;
	conf.mode = I2C_MODE_MASTER;
	conf.sda_io_num = 18;
	conf.scl_io_num = 19;
	conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
	conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
	conf.master.clk_speed = 100000;
	i2c_param_config(I2C_NUM_0, &conf);

	i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

  dev.dev_id = BME280_I2C_ADDR_SEC;
  dev.intf = BME280_I2C_INTF;
  dev.read = user_i2c_read;
  dev.write = user_i2c_write;
  dev.delay_ms = user_delay_ms;

  int8_t rslt = bme280_init(&dev);
  //printf("rslt: %d\n", rslt);

	while(1) {
    stream_sensor_data_forced_mode(&dev);
		vTaskDelay(1000/portTICK_PERIOD_MS);
	}
	vTaskDelete(NULL);
}

void app_main()
{
    printf("Hello world!\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    xTaskCreate(&task_bme280, "task_bme280", 8192, NULL, 5, NULL);
}