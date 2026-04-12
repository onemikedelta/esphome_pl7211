#include "pl7211.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pl7211 {

static const char *const TAG = "pl7211";

void PL7211Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up PL7211...");
  
  this->spi_setup(); // Initialize ESPHome SPI bus

  // 1. Take control of GPIO from DSP to CFG Register (0x380D = 0xF7)
  this->write_register(0x380D, 0xF7);

  // 2. Set GPIO 9, 12, 13 as Outputs in DIR Register (0x3903)
  // GPIO9 = Bit 1, GPIO12 = Bit 4, GPIO13 = Bit 5. 
  // (1<<1) | (1<<4) | (1<<5) = 0x32
  this->write_register(0x3903, 0x32);
}

void PL7211Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PL7211 Component:");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_SENSOR("  ", "Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Current", this->current_sensor_);
  LOG_SENSOR("  ", "Power", this->power_sensor_);
}

void PL7211Component::write_register(uint16_t addr, uint8_t data) {
  this->enable(); // Automatically pulls CS low
  this->write_byte(0x41); // Command: Write, No CRC, 1 Byte
  this->write_byte(addr >> 8);
  this->write_byte(addr & 0xFF);
  this->write_byte(data);
  this->disable(); // Automatically pulls CS high
}

uint32_t PL7211Component::read_register_32(uint16_t addr) {
  this->enable();
  this->write_byte(0x34); // Command: Read, No CRC, 4 Bytes
  this->write_byte(addr >> 8);
  this->write_byte(addr & 0xFF);
  
  uint32_t val = 0;
  val |= (uint32_t)this->read_byte() << 24;
  val |= (uint32_t)this->read_byte() << 16;
  val |= (uint32_t)this->read_byte() << 8;
  val |= (uint32_t)this->read_byte();
  
  this->disable();
  return val;
}

float PL7211Component::read_register_float(uint16_t addr) {
  uint32_t raw_val = this->read_register_32(addr);
  // PL7211 DSP converts internally to standard IEEE-754 32-bit Float
  float float_val;
  memcpy(&float_val, &raw_val, sizeof(float_val));
  return float_val;
}

void PL7211Component::set_gpio_bit(uint8_t bit, bool state) {
  if (state) {
    this->gpio_data_high_ |= (1 << bit);
  } else {
    this->gpio_data_high_ &= ~(1 << bit);
  }
  // Write to GPIO DATA High byte (0x3905)
  this->write_register(0x3905, this->gpio_data_high_);
}

void PL7211Component::update() {
  // Read and publish DSP Energy registers (Addresses from App Note Table 6-1)
  if (this->voltage_sensor_ != nullptr) {
    float v = this->read_register_float(0x3078);
    this->voltage_sensor_->publish_state(v);
  }
  if (this->current_sensor_ != nullptr) {
    float c = this->read_register_float(0x3084);
    this->current_sensor_->publish_state(c);
  }
  if (this->power_sensor_ != nullptr) {
    float p = this->read_register_float(0x3090);
    this->power_sensor_->publish_state(p);
  }
  if (this->power_factor_sensor_ != nullptr) {
    float pf = this->read_register_float(0x30B4);
    this->power_factor_sensor_->publish_state(pf);
  }
  if (this->frequency_sensor_ != nullptr) {
    float f = this->read_register_float(0x30C0);
    this->frequency_sensor_->publish_state(f);
  }
}

}  // namespace pl7211
}  // namespace esphome