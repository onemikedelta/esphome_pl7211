#include "pl7211.h"
#include "esphome/core/log.h"

namespace esphome {
namespace pl7211 {

static const char *const TAG = "pl7211";

void PL7211Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up PL7211...");
  this->spi_setup();
  
  // PL7211'in kendi DSP'sinin uyanıp MTP hafızasını yüklemesi için kısa bir bekleme
  delay(150); 

  // 1. GPIO kontrolünü DSP'den al, SPI CFG Register'ına ver (App Note Syf: 28)
  this->write_register_8(0x380D, 0xF7);

  // 2. GPIO 9, 12, 13'ü Çıkış (Output) yap. (16-bit register adresi: 0x3902)
  uint16_t dir_mask = (1 << 9) | (1 << 12) | (1 << 13);
  this->write_register_16(0x3902, dir_mask);
}

void PL7211Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PL7211 Component Setup Complete.");
}

void PL7211Component::write_register_8(uint16_t addr, uint8_t data) {
  this->enable(); 
  this->write_byte(0x41); // Komut 0x41: Yazma, CRC Yok, 1 Byte
  this->write_byte(addr >> 8);
  this->write_byte(addr & 0xFF);
  this->write_byte(data);
  this->disable();
}

void PL7211Component::write_register_16(uint16_t addr, uint16_t data) {
  this->enable();
  this->write_byte(0x42); // Komut 0x42: Yazma, CRC Yok, 2 Byte
  this->write_byte(addr >> 8);
  this->write_byte(addr & 0xFF);
  this->write_byte(data & 0xFF); // Low Byte ilk gider
  this->write_byte(data >> 8);   // High Byte sonra gider
  this->disable();
}

uint16_t PL7211Component::read_register_16(uint16_t addr) {
  this->enable();
  this->write_byte(0x32); // Komut 0x32: Okuma, CRC Yok, 2 Byte
  this->write_byte(addr >> 8);
  this->write_byte(addr & 0xFF);
  uint16_t val = 0;
  val |= this->read_byte();            // Low Byte
  val |= (this->read_byte() << 8);     // High Byte
  this->disable();
  return val;
}

uint64_t PL7211Component::read_register_48(uint16_t addr) {
  this->enable();
  this->write_byte(0x36); // Komut 0x36: Okuma, CRC Yok, 6 Byte
  this->write_byte(addr >> 8);
  this->write_byte(addr & 0xFF);
  uint64_t val = 0;
  // App Note Syf 59: Data[0] Lowest Byte, Data[5] Highest Byte'tır
  for (int i = 0; i < 6; i++) {
    uint64_t b = this->read_byte();
    val |= (b << (i * 8));
  }
  this->disable();
  return val;
}

void PL7211Component::set_gpio_bit(uint8_t bit, bool state) {
  // Bu fonksiyon artık doğrudan 0-15 arası pin numarasını kabul eder
  if (state) {
    this->gpio_state_ |= (1 << bit);
  } else {
    this->gpio_state_ &= ~(1 << bit);
  }
  // Güvenlik için 16-bit veri adresine (0x3904) blok yazma yapıyoruz
  this->write_register_16(0x3904, this->gpio_state_);
}

void PL7211Component::update() {
  // Voltaj (App Note Syf: 59)
  if (this->voltage_sensor_ != nullptr) {
    uint64_t v_raw = this->read_register_48(0x3078);
    float vrms = (float)v_raw / 16777216.0f; // Formül: Değer / 2^24
    this->voltage_sensor_->publish_state(vrms);
  }
  
  // Akım (App Note Syf: 60)
  if (this->current_sensor_ != nullptr) {
    uint64_t i_raw = this->read_register_48(0x3084);
    float irms = (float)i_raw / 1073741824.0f; // Formül: Değer / 2^30
    this->current_sensor_->publish_state(irms);
  }
  
  // Aktif Güç (App Note Syf: 61)
  if (this->power_sensor_ != nullptr) {
    uint64_t p_raw = this->read_register_48(0x3090);
    float power = (float)p_raw / 16777216.0f; // Formül: Değer / 2^24
    this->power_sensor_->publish_state(power);
  }
  
  // Frekans (App Note Syf: 64)
  if (this->frequency_sensor_ != nullptr) {
    uint16_t zcc_cnt = this->read_register_16(0x3018);
    uint64_t zcc_start = this->read_register_48(0x301E);
    uint64_t zcc_stop = this->read_register_48(0x3024);
    uint16_t sample_cnt = this->read_register_16(0x3809);
    
    // Sıfıra bölünme hatasını engellemek için güvenlik kontrolü
    if (sample_cnt > 0 && zcc_stop > zcc_start && zcc_cnt > 1) {
      float time_diff = (float)(zcc_stop - zcc_start) / (float)sample_cnt;
      if (time_diff > 0.0f) {
        float freq = ((zcc_cnt - 1) / 2.0f) / time_diff;
        this->frequency_sensor_->publish_state(freq);
      }
    }
  }

  // Güç Faktörü (App Note Syf: 62)
  if (this->power_factor_sensor_ != nullptr) {
    float vrms = this->voltage_sensor_->state;
    float irms = this->current_sensor_->state;
    float power = this->power_sensor_->state;
    
    if (vrms > 0.0f && irms > 0.0f) {
      float pf = power / (vrms * irms);
      if (pf > 1.0f) pf = 1.0f; // Sınır koruması
      this->power_factor_sensor_->publish_state(pf);
    }
  }
}

}  // namespace pl7211
}  // namespace esphome