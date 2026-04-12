#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace pl7211 {

// The PL7211 uses SPI Mode 3 (CPOL=1, CPHA=1) and we operate at 1MHz
class PL7211Component : public PollingComponent,
                        public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH, spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_1MHZ> {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  // Sensor Setters
  void set_voltage_sensor(sensor::Sensor *s) { voltage_sensor_ = s; }
  void set_current_sensor(sensor::Sensor *s) { current_sensor_ = s; }
  void set_power_sensor(sensor::Sensor *s) { power_sensor_ = s; }
  void set_power_factor_sensor(sensor::Sensor *s) { power_factor_sensor_ = s; }
  void set_frequency_sensor(sensor::Sensor *s) { frequency_sensor_ = s; }

  // SPI Communication methods
  void write_register(uint16_t addr, uint8_t data);
  uint32_t read_register_32(uint16_t addr);
  float read_register_float(uint16_t addr);

  // GPIO control method
  void set_gpio_bit(uint8_t bit, bool state);

 protected:
  uint8_t gpio_data_high_{0x00}; // Stores the state of GPIO8-GPIO15 (Register 0x3905)

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