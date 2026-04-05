#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace spl06 {

// Register addresses
static const uint8_t SPL06_REG_PSR_B2 = 0x00;
static const uint8_t SPL06_REG_PSR_B1 = 0x01;
static const uint8_t SPL06_REG_PSR_B0 = 0x02;
static const uint8_t SPL06_REG_TMP_B2 = 0x03;
static const uint8_t SPL06_REG_TMP_B1 = 0x04;
static const uint8_t SPL06_REG_TMP_B0 = 0x05;
static const uint8_t SPL06_REG_PRS_CFG = 0x06;
static const uint8_t SPL06_REG_TMP_CFG = 0x07;
static const uint8_t SPL06_REG_MEAS_CFG = 0x08;
static const uint8_t SPL06_REG_CFG_REG = 0x09;
static const uint8_t SPL06_REG_ID = 0x0D;
static const uint8_t SPL06_REG_COEF = 0x10;  // 0x10-0x21 (18 bytes)
static const uint8_t SPL06_REG_RESET = 0x0C;

// Chip ID
static const uint8_t SPL06_CHIP_ID = 0x10;

class SPL06Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;
  float get_setup_priority() const override;

  void set_temperature_sensor(sensor::Sensor *sensor) { temperature_sensor_ = sensor; }
  void set_pressure_sensor(sensor::Sensor *sensor) { pressure_sensor_ = sensor; }

 protected:
  bool read_coefficients_();
  int32_t read_raw_temperature_();
  int32_t read_raw_pressure_();
  double get_scale_factor_(uint8_t oversampling_bits);

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};

  // Calibration coefficients
  int16_t c0_, c1_;
  int32_t c00_, c10_;
  int16_t c01_, c11_, c20_, c21_, c30_;

  // Scaling factors
  double kt_scale_;
  double kp_scale_;
};

}  // namespace spl06
}  // namespace esphome
