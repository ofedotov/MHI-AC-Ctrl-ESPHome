#include "esphome/core/log.h"
#include "mhi_climate.h"

namespace esphome {
namespace mhi {

static const char *const TAG = "mhi.climate";

void MhiClimate::setup() {
  this->parent_->add_listener(this);
}

void MhiClimate::dump_config() {
  ESP_LOGCONFIG(TAG, "MHI Climate");
  this->dump_traits_(TAG);
}

void MhiClimate::on_status(ACStatus status, int value) {
  switch (status) {
    case status_power:
      if (value == power_off) {
        this->mode = climate::CLIMATE_MODE_OFF;
        this->action = climate::CLIMATE_ACTION_OFF;
      }
      this->publish_state();
      break;

    case status_mode:
      switch (value) {
        case mode_auto:
          this->mode = climate::CLIMATE_MODE_HEAT_COOL;
          this->action = climate::CLIMATE_ACTION_IDLE;
          break;
        case mode_dry:
          this->mode = climate::CLIMATE_MODE_DRY;
          this->action = climate::CLIMATE_ACTION_DRYING;
          break;
        case mode_cool:
          this->mode = climate::CLIMATE_MODE_COOL;
          this->action = climate::CLIMATE_ACTION_COOLING;
          break;
        case mode_fan:
          this->mode = climate::CLIMATE_MODE_FAN_ONLY;
          this->action = climate::CLIMATE_ACTION_FAN;
          break;
        case mode_heat:
          this->mode = climate::CLIMATE_MODE_HEAT;
          this->action = climate::CLIMATE_ACTION_HEATING;
          break;
        default:
          break;
      }
      this->publish_state();
      break;

    case status_fan:
      // 3-Speed 20-byte MHI mapping:
      // Remote Low (1) -> LOW | Remote Medium (2) -> MEDIUM | Remote Hi (3) -> HIGH | Auto (7) -> AUTO
      switch (value) {
        case 0:
        case 1:
          this->fan_mode = climate::CLIMATE_FAN_LOW;
          break;
        case 2:
          this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
          break;
        case 3:
        case 4:
        case 6:
          this->fan_mode = climate::CLIMATE_FAN_HIGH;
          break;
        case 7:
        default:
          this->fan_mode = climate::CLIMATE_FAN_AUTO;
          break;
      }
      this->publish_state();
      break;

    case status_troom:
      this->current_temperature = (value - 61) / 4.0f;
      this->publish_state();
      break;

    case status_tsetpoint:
      this->target_temperature = (value & 0x7f) / 2.0f;
      this->publish_state();
      break;

    default:
      break;
  }
}

void MhiClimate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) {
    this->mode = *call.get_mode();
    switch (this->mode) {
      case climate::CLIMATE_MODE_OFF:
        this->parent_->set_power(false);
        break;
      case climate::CLIMATE_MODE_HEAT_COOL:
        this->parent_->set_power(true);
        this->parent_->set_mode(mode_auto);
        break;
      case climate::CLIMATE_MODE_COOL:
        this->parent_->set_power(true);
        this->parent_->set_mode(mode_cool);
        break;
      case climate::CLIMATE_MODE_HEAT:
        this->parent_->set_power(true);
        this->parent_->set_mode(mode_heat);
        break;
      case climate::CLIMATE_MODE_DRY:
        this->parent_->set_power(true);
        this->parent_->set_mode(mode_dry);
        break;
      case climate::CLIMATE_MODE_FAN_ONLY:
        this->parent_->set_power(true);
        this->parent_->set_mode(mode_fan);
        break;
      default:
        break;
    }
  }

  if (call.get_target_temperature().has_value()) {
    this->target_temperature = *call.get_target_temperature();
    this->parent_->set_tsetpoint(this->target_temperature);
  }

  if (call.get_fan_mode().has_value()) {
    this->fan_mode = *call.get_fan_mode();
    switch (this->fan_mode.value()) {
      case climate::CLIMATE_FAN_LOW:
        this->parent_->set_fan(1);  // Sends Speed 1 (Remote: Low)
        break;
      case climate::CLIMATE_FAN_MEDIUM:
        this->parent_->set_fan(2);  // Sends Speed 2 (Remote: Medium)
        break;
      case climate::CLIMATE_FAN_HIGH:
        this->parent_->set_fan(3);  // Sends Speed 3 (Remote: Hi / Max)
        break;
      case climate::CLIMATE_FAN_AUTO:
      default:
        this->parent_->set_fan(7);  // Sends Auto (Remote: Auto)
        break;
    }
  }

  this->publish_state();
}

climate::ClimateTraits MhiClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(true);
  traits.set_supports_action(true);
  traits.set_visual_min_temperature(this->minimum_temperature_);
  traits.set_visual_max_temperature(30.0f);
  traits.set_visual_temperature_step(0.5f);

  traits.set_supported_modes({
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_HEAT_COOL,
      climate::CLIMATE_MODE_COOL,
      climate::CLIMATE_MODE_HEAT,
      climate::CLIMATE_MODE_DRY,
      climate::CLIMATE_MODE_FAN_ONLY,
  });

  traits.set_supported_fan_modes({
      climate::CLIMATE_FAN_AUTO,
      climate::CLIMATE_FAN_LOW,
      climate::CLIMATE_FAN_MEDIUM,
      climate::CLIMATE_FAN_HIGH,
  });

  return traits;
}

}  // namespace mhi
}  // namespace esphome
