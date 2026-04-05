#include "spl06.h"
#include "esphome/core/log.h"

namespace esphome {
namespace spl06 {

static const char *const TAG = "spl06";

float SPL06Component::get_setup_priority() const { return setup_priority::DATA; }

double SPL06Component::get_scale_factor_(uint8_t oversampling_bits) {
  switch (oversampling_bits & 0x07) {
    case 0: return 524288.0;
    case 1: return 1572864.0;
    case 2: return 3670016.0;
    case 3: return 7864320.0;
    case 4: return 253952.0;
    case 5: return 516096.0;
    case 6: return 1040384.0;
    case 7: return 2088960.0;
    default: return 524288.0;
  }
}

void SPL06Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SPL06...");

  // Read chip ID
  uint8_t chip_id;
  if (!this->read_byte(SPL06_REG_ID, &chip_id)) {
    ESP_LOGE(TAG, "Failed to read chip ID");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "Chip ID: 0x%02X", chip_id);
  if (chip_id != SPL06_CHIP_ID) {
    ESP_LOGE(TAG, "Unknown chip ID 0x%02X, expected 0x%02X", chip_id, SPL06_CHIP_ID);
    this->mark_failed();
    return;
  }

  // Soft reset
  this->write_byte(SPL06_REG_RESET, 0x89);
  delay(50);

  // Wait for coefficients to be ready
  uint8_t meas_cfg;
  for (int i = 0; i < 10; i++) {
    this->read_byte(SPL06_REG_MEAS_CFG, &meas_cfg);
    if (meas_cfg & 0x80) break;  // COEF_RDY bit
    delay(10);
  }

  if (!(meas_cfg & 0x80)) {
    ESP_LOGE(TAG, "Coefficients not ready");
    this->mark_failed();
    return;
  }

  // Read calibration coefficients
  if (!this->read_coefficients_()) {
    ESP_LOGE(TAG, "Failed to read calibration coefficients");
    this->mark_failed();
    return;
  }

  // Configure: 8x oversampling for both pressure and temperature
  // PRS_CFG: rate=1/s (bits 6:4 = 000), oversampling=8x (bits 3:0 = 0011)
  this->write_byte(SPL06_REG_PRS_CFG, 0x03);
  // TMP_CFG: external sensor (bit 7=1), rate=1/s, oversampling=8x
  this->write_byte(SPL06_REG_TMP_CFG, 0x83);
  // CFG_REG: enable pressure and temperature shift (needed when oversampling > 8x... set anyway)
  this->write_byte(SPL06_REG_CFG_REG, 0x00);
  // MEAS_CFG: continuous pressure and temperature measurement
  this->write_byte(SPL06_REG_MEAS_CFG, 0x07);

  // Read back config to determine scale factors
  uint8_t prs_cfg, tmp_cfg;
  this->read_byte(SPL06_REG_PRS_CFG, &prs_cfg);
  this->read_byte(SPL06_REG_TMP_CFG, &tmp_cfg);
  kp_scale_ = get_scale_factor_(prs_cfg & 0x07);
  kt_scale_ = get_scale_factor_(tmp_cfg & 0x07);

  ESP_LOGCONFIG(TAG, "SPL06 initialized successfully");
}

bool SPL06Component::read_coefficients_() {
  uint8_t coef[18];
  // Read 18 bytes of coefficients starting at 0x10
  if (!this->read_bytes(SPL06_REG_COEF, coef, 18)) {
    return false;
  }

  // c0: 12-bit signed from coef[0] and upper 4 bits of coef[1]
  c0_ = ((int16_t) coef[0] << 4) | ((coef[1] >> 4) & 0x0F);
  if (c0_ & 0x0800) c0_ |= 0xF000;  // sign extend

  // c1: 12-bit signed from lower 4 bits of coef[1] and coef[2]
  c1_ = ((int16_t)(coef[1] & 0x0F) << 8) | coef[2];
  if (c1_ & 0x0800) c1_ |= 0xF000;

  // c00: 20-bit signed from coef[3], coef[4], upper 4 bits of coef[5]
  c00_ = ((int32_t) coef[3] << 12) | ((int32_t) coef[4] << 4) | ((coef[5] >> 4) & 0x0F);
  if (c00_ & 0x80000) c00_ |= 0xFFF00000;

  // c10: 20-bit signed from lower 4 bits of coef[5], coef[6], coef[7]
  c10_ = ((int32_t)(coef[5] & 0x0F) << 16) | ((int32_t) coef[6] << 8) | coef[7];
  if (c10_ & 0x80000) c10_ |= 0xFFF00000;

  // c01: 16-bit signed from coef[8], coef[9]
  c01_ = ((int16_t) coef[8] << 8) | coef[9];

  // c11: 16-bit signed from coef[10], coef[11]
  c11_ = ((int16_t) coef[10] << 8) | coef[11];

  // c20: 16-bit signed from coef[12], coef[13]
  c20_ = ((int16_t) coef[12] << 8) | coef[13];

  // c21: 16-bit signed from coef[14], coef[15]
  c21_ = ((int16_t) coef[14] << 8) | coef[15];

  // c30: 16-bit signed from coef[16], coef[17]
  c30_ = ((int16_t) coef[16] << 8) | coef[17];

  ESP_LOGD(TAG, "Coefficients: c0=%d c1=%d c00=%d c10=%d c01=%d c11=%d c20=%d c21=%d c30=%d",
           c0_, c1_, c00_, c10_, c01_, c11_, c20_, c21_, c30_);

  return true;
}

int32_t SPL06Component::read_raw_temperature_() {
  uint8_t data[3];
  if (!this->read_bytes(SPL06_REG_TMP_B2, data, 3)) {
    return 0;
  }
  int32_t raw = ((int32_t) data[0] << 16) | ((int32_t) data[1] << 8) | data[2];
  if (raw & 0x800000) raw |= 0xFF000000;  // sign extend 24-bit to 32-bit
  return raw;
}

int32_t SPL06Component::read_raw_pressure_() {
  uint8_t data[3];
  if (!this->read_bytes(SPL06_REG_PSR_B2, data, 3)) {
    return 0;
  }
  int32_t raw = ((int32_t) data[0] << 16) | ((int32_t) data[1] << 8) | data[2];
  if (raw & 0x800000) raw |= 0xFF000000;
  return raw;
}

void SPL06Component::update() {
  // Wait for sensor ready
  uint8_t meas_cfg;
  this->read_byte(SPL06_REG_MEAS_CFG, &meas_cfg);
  if (!(meas_cfg & 0x20)) {  // TMP_RDY
    ESP_LOGW(TAG, "Temperature not ready");
  }
  if (!(meas_cfg & 0x10)) {  // PRS_RDY
    ESP_LOGW(TAG, "Pressure not ready");
  }

  // Read and calculate temperature
  int32_t traw = read_raw_temperature_();
  double traw_sc = (double) traw / kt_scale_;
  double temperature = (double) c0_ * 0.5 + (double) c1_ * traw_sc;

  // Read and calculate pressure
  int32_t praw = read_raw_pressure_();
  double praw_sc = (double) praw / kp_scale_;

  double pressure = (double) c00_ +
                    praw_sc * ((double) c10_ + praw_sc * ((double) c20_ + praw_sc * (double) c30_)) +
                    traw_sc * (double) c01_ +
                    traw_sc * praw_sc * ((double) c11_ + praw_sc * (double) c21_);

  // Convert Pa to hPa
  pressure /= 100.0;

  ESP_LOGD(TAG, "Temperature: %.1f °C, Pressure: %.1f hPa", temperature, pressure);

  if (temperature_sensor_ != nullptr)
    temperature_sensor_->publish_state(temperature);
  if (pressure_sensor_ != nullptr)
    pressure_sensor_->publish_state(pressure);
}

void SPL06Component::dump_config() {
  ESP_LOGCONFIG(TAG, "SPL06:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
}

}  // namespace spl06
}  // namespace esphome
