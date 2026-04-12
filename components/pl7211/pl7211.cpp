#include "pl7211.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pl7211 {

static const char *const TAG = "pl7211";

void PL7211Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up PL7211...");
  this->spi_setup();
  
  // Power-on delay to let the internal DSP/MTP load
  delay(200); 

  // Enable SPI access to GPIOs by writing to SPI_CFG register (0x380D)
  // Setting bit 3 low allows SPI to control GPIOs instead of DSP
  this->write_register_8(0x380D, 0x10); 
  delay(10);

  // Set GPIO 9, 12, and 13 as outputs
  // Register 0x3902 is GPIO_DIR_L (0-7), 0x3903 is GPIO_DIR_H (8-15)
  // GPIO 9  -> Bit 1 of 0x3903
  // GPIO 12 -> Bit 4 of 0x3903
  // GPIO 13 -> Bit 5 of 0x3903
  uint8_t dir_h = (1 << (9-8)) | (1 << (12-8)) | (1 << (13-8));
  this->write_register_8(0x3902, 0x00); // Pins 0-7 as inputs
  this->write_register_8(0x3903, dir_h); // Specific pins in 8-15 as outputs
  
  ESP_LOGD(TAG, "GPIO Directions configured. DIR_H: 0x%02X", dir_h);
}

void PL7211Component::write_register_8(uint16_t addr, uint8_t data) {
  this->enable(); 
  // Command 0x41: Write, No CRC, 1 Byte of data
  this->write_byte(0x41); 
  this->write_byte(addr >> 8);
  this->write_byte(addr & 0xFF);
  this->write_byte(data);
  this->disable();
}

void PL7211Component::set_gpio_bit(uint8_t bit, bool state) {
  // Update local 16-bit state buffer
  if (state) {
    this->gpio_state_ |= (1 << bit);
  } else {
    this->gpio_state_ &= ~(1 << bit);
  }

  // PL7211 stores GPIO states in 0x3904 (Low) and 0x3905 (High)
  // We write them separately to ensure the internal state machine updates correctly
  uint8_t low_byte = this->gpio_state_ & 0xFF;
  uint8_t high_byte = (this->gpio_state_ >> 8) & 0xFF;

  this->write_register_8(0x3904, low_byte);
  this->write_register_8(0x3905, high_byte);
  
  ESP_LOGD(TAG, "GPIO State Updated: 0x%04X (Bit %d set to %d)", this->gpio_state_, bit, state);
}

// ... read_register_16, read_register_48 and update() functions remain the same as previous logic
// but with English comments for consistency in your file ...

uint16_t PL7211Component::read_register_16(uint16_t addr) {
  this->enable();
  // Command 0x32: Read, No CRC, 2 Bytes
  this->write_byte(0x32);
  this->write_byte(addr >> 8);
  this->write_byte(addr & 0xFF);
  uint16_t val = 0;
  val |= this->read_byte();            // Read Low Byte
  val |= (this->read_byte() << 8);     // Read High Byte
  this->disable();
  return val;
}

void PL7211Component::update() {
  // Logic for sensors...
  // All math based on Application Note formulas (Integer to Float conversion)
  
  if (this->voltage_sensor_ != nullptr) {
    uint64_t v_raw = this->read_register_48(0x3078);
    float vrms = (float)v_raw / 16777216.0f; // Scale by 2^24
    this->voltage_sensor_->publish_state(vrms);
  }
  
  if (this->frequency_sensor_ != nullptr) {
    uint16_t zcc_cnt = this->read_register_16(0x3018);
    uint64_t zcc_start = this->read_register_48(0x301E);
    uint64_t zcc_stop = this->read_register_48(0x3024);
    uint16_t sample_cnt = this->read_register_16(0x3809);
    
    if (sample_cnt > 0 && zcc_stop > zcc_start && zcc_cnt > 1) {
      float time_diff = (float)(zcc_stop - zcc_start) / (float)sample_cnt;
      if (time_diff > 0.0f) {
        float freq = ((zcc_cnt - 1) / 2.0f) / time_diff;
        this->frequency_sensor_->publish_state(freq);
      }
    } else {
        this->frequency_sensor_->publish_state(0.0f); // Default to 0 if no AC detected
    }
  }
}

}  // namespace pl7211
}  // namespace esphome