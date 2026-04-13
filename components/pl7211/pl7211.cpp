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

  this->write_register_8(0x3803, 0x00);
  this->write_register_8(0x3804, 0x00);
  delay(10);

  uint8_t iocfg = this->read_register_8(0x380D);
  iocfg &= ~(1 << 3); // 3. Biti sıfırla (0: Register kontrolü, 1: DSP kontrolü)
  this->write_register_8(0x380D, iocfg); 
  delay(10);

  // Register 0x3902 GPIO_DIR_L (0-7), 0x3903 GPIO_DIR_H (8-15)
  // GPIO 9  -> Bit 1 of 0x3903 (bit 9-8 = 1)
  // GPIO 12 -> Bit 4 of 0x3903 (bit 12-8 = 4)
  // GPIO 13 -> Bit 5 of 0x3903 (bit 13-8 = 5)
  uint8_t dir_h = (1 << 1) | (1 << 4) | (1 << 5); 
  this->write_register_8(0x3902, 0x00);  // Pin 0-7 Input
  this->write_register_8(0x3903, dir_h); // Pin 9, 12, 13 Output
  
  ESP_LOGD(TAG, "GPIO Directions configured. DIR_H: 0x%02X", dir_h);
}

void PL7211Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PL7211:");
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_UPDATE_INTERVAL(this);
}

uint8_t PL7211Component::read_register_8(uint16_t addr) {
  this->enable();
  // Command 0x31: Read, No CRC, 1 Byte
  this->write_byte(0x31);
  this->write_byte(addr >> 8);
  this->write_byte(addr & 0xFF);
  uint8_t val = this->read_byte();
  this->disable();
  return val;
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

uint16_t PL7211Component::read_register_16(uint16_t addr) {
  this->enable();
  // Command 0x32: Read, No CRC, 2 Bytes
  this->write_byte(0x32);
  this->write_byte(addr >> 8);
  this->write_byte(addr & 0xFF);
  uint16_t val = 0;
  val |= this->read_byte();                  // Read Low Byte
  val |= (uint16_t(this->read_byte()) << 8); // Read High Byte
  this->disable();
  return val;
}

uint64_t PL7211Component::read_register_48(uint16_t addr) {
  this->enable();
  // Command 0x36: Read, No CRC, 6 Bytes
  this->write_byte(0x36);
  this->write_byte(addr >> 8);
  this->write_byte(addr & 0xFF);
  uint64_t val = 0;
  // PL7211 sends 48-bit data in 6-byte chunks (Little Endian)
  for (int i = 0; i < 6; i++) {
    uint64_t b = this->read_byte();
    val |= (b << (i * 8));
  }
  this->disable();
  return val;
}

void PL7211Component::set_gpio_bit(uint8_t bit, bool state) {
  if (state) {
    this->gpio_state_ |= (1 << bit);
  } else {
    this->gpio_state_ &= ~(1 << bit);
  }

  // Update GPIO registers separately for stability
  uint8_t low_byte = this->gpio_state_ & 0xFF;
  uint8_t high_byte = (this->gpio_state_ >> 8) & 0xFF;

  this->write_register_8(0x3904, low_byte);
  this->write_register_8(0x3905, high_byte);
  
  ESP_LOGD(TAG, "GPIO State Updated: 0x%04X", this->gpio_state_);
}

void PL7211Component::update() {
  // Voltage Calculation (App Note Page 59)
  if (this->voltage_sensor_ != nullptr) {
    uint64_t v_raw = this->read_register_48(0x3078);
    float vrms = (float)v_raw / 16777216.0f; // V_raw / 2^24
    this->voltage_sensor_->publish_state(vrms);
  }
  
  // Current Calculation (App Note Page 60)
  if (this->current_sensor_ != nullptr) {
    uint64_t i_raw = this->read_register_48(0x3084);
    float irms = (float)i_raw / 1073741824.0f; // I_raw / 2^30
    this->current_sensor_->publish_state(irms);
  }

  // Active Power Calculation (App Note Page 61)
  if (this->power_sensor_ != nullptr) {
    uint64_t p_raw = this->read_register_48(0x3090);
    float p_val = (float)p_raw / 16777216.0f; // P_raw / 2^24
    this->power_sensor_->publish_state(p_val);
  }

  // Frequency Calculation (App Note Page 64)
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
      this->frequency_sensor_->publish_state(0.0f);
    }
  }

  // Power Factor Calculation (App Note Page 62)
  if (this->power_factor_sensor_ != nullptr && this->voltage_sensor_->has_state() && this->current_sensor_->has_state()) {
    float v = this->voltage_sensor_->state;
    float i = this->current_sensor_->state;
    float p = this->power_sensor_->state;
    if (v > 10.0f && i > 0.001f) {
        float pf = p / (v * i);
        this->power_factor_sensor_->publish_state(std::min(1.0f, std::max(-1.0f, pf)));
    }
  }
}

}  // namespace pl7211
}  // namespace esphome