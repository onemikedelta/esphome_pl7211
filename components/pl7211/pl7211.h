#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/spi/spi.h"
#include <cstdint>

namespace esphome {
namespace pl7211 {

class PL7211Component : public PollingComponent,
                        public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH, spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_voltage_sensor(sensor::Sensor *s) { voltage_sensor_ = s; }
  void set_current_sensor(sensor::Sensor *s) { current_sensor_ = s; }
  void set_power_sensor(sensor::Sensor *s) { power_sensor_ = s; }
  void set_power_factor_sensor(sensor::Sensor *s) { power_factor_sensor_ = s; }
  void set_frequency_sensor(sensor::Sensor *s) { frequency_sensor_ = s; }

  // 8-bit, 16-bit ve 48-bit SPI Okuma/Yazma Komutları
  void write_register_8(uint16_t addr, uint8_t data);
  void write_register_16(uint16_t addr, uint16_t data);
  uint16_t read_register_16(uint16_t addr);
  uint64_t read_register_48(uint16_t addr);

  void set_gpio_bit(uint8_t bit, bool state);

 protected:
  uint16_t gpio_state_{0x0000}; // GPIO0-GPIO15 durumlarını tutan 16-bit hafıza

  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
  sensor::Sensor *power_factor_sensor_{nullptr};
  sensor::Sensor *frequency_sensor_{nullptr};
};

class PL7211Switch : public switch_::Switch, public Component {
 public:
  void set_parent(PL7211Component *parent) { parent_ = parent; }
  void set_bit(uint8_t bit) { bit_ = bit; }

 protected:
  void write_state(bool state) override {
    parent_->set_gpio_bit(bit_, state);
    publish_state(state);
  }
  PL7211Component *parent_;
  uint8_t bit_;
};

}  // namespace pl7211
}  // namespace esphome