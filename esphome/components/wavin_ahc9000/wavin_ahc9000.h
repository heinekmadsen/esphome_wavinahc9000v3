#pragma once

#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <string>

namespace esphome {
namespace sensor { class Sensor; }
namespace text_sensor { class TextSensor; }
namespace binary_sensor { class BinarySensor; }
namespace switch_ { class Switch; }
namespace wavin_ahc9000 {

// Forward declarations
class WavinZoneClimate;
class WavinChildLockSwitch;

/**
 * Wavin AHC9000 Modbus Controller Integration for ESPHome
 * 
 * Supports up to 16 heating zones via RS485 Modbus protocol.
 * Features: climate control, battery monitoring, floor temperature limits,
 * communication health tracking, per-channel online detection, and valve feedback.
 */
class WavinAHC9000 : public PollingComponent, public uart::UARTDevice {
 public:
  // Configuration
  void set_temp_divisor(float d) { this->temp_divisor_ = d; }
  void set_receive_timeout_ms(uint32_t t) { this->receive_timeout_ms_ = t; }
  void set_tx_enable_pin(GPIOPin *p) { this->tx_enable_pin_ = p; }
  /// Optional half-duplex RS485 DE/RE (flow control) pin. HIGH=transmit, LOW=receive.
  void set_flow_control_pin(GPIOPin *p) { this->flow_control_pin_ = p; }
  void set_poll_channels_per_cycle(uint8_t n) { this->poll_channels_per_cycle_ = n == 0 ? 1 : (n > 16 ? 16 : n); }
  void set_allow_mode_writes(bool v) { this->allow_mode_writes_ = v; }
  bool get_allow_mode_writes() const { return this->allow_mode_writes_; }
  /// Friendly name per channel (optional, for generated YAML)
  void set_channel_friendly_name(uint8_t channel, const std::string &name);
  std::string get_channel_friendly_name(uint8_t channel) const;

  // Lifecycle
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;

  // Component registration
  void add_channel_climate(WavinZoneClimate *c);
  void add_group_climate(WavinZoneClimate *c);
  void add_channel_battery_sensor(uint8_t ch, sensor::Sensor *s);
  void add_channel_temperature_sensor(uint8_t ch, sensor::Sensor *s);
  void add_channel_comfort_setpoint_sensor(uint8_t ch, sensor::Sensor *s);
  void add_channel_floor_temperature_sensor(uint8_t ch, sensor::Sensor *s);
  void add_channel_floor_min_temperature_sensor(uint8_t ch, sensor::Sensor *s);
  void add_channel_floor_max_temperature_sensor(uint8_t ch, sensor::Sensor *s);
  /// NEW: Valve position sensor (0-100%)
  void add_channel_output_sensor(uint8_t ch, sensor::Sensor *s);
  /// NEW: Channel online/offline binary sensor
  void add_channel_online_sensor(uint8_t ch, binary_sensor::BinarySensor *s);
  void add_channel_child_lock_switch(uint8_t ch, switch_::Switch *s) { this->child_lock_switches_[ch] = s; }
  /// NEW: Modbus communication health percentage sensor
  void set_comm_health_sensor(sensor::Sensor *s) { this->comm_health_sensor_ = s; }
  void add_active_channel(uint8_t ch);

  // Commands
  void write_channel_setpoint(uint8_t channel, float celsius);
  void write_group_setpoint(const std::vector<uint8_t> &members, float celsius);
  void write_channel_mode(uint8_t channel, climate::ClimateMode mode);
  void write_channel_child_lock(uint8_t channel, bool enable);
  void write_channel_floor_min_temperature(uint8_t channel, float celsius);
  void write_channel_floor_max_temperature(uint8_t channel, float celsius);
  void refresh_channel_now(uint8_t channel);
  void set_strict_mode_write(uint8_t channel, bool enable);
  bool is_strict_mode_write(uint8_t channel) const;
  void request_status();
  void request_status_channel(uint8_t ch_index);
  void normalize_channel_config(uint8_t channel, bool off);
  void generate_yaml_suggestion();
  void set_yaml_ready_binary_sensor(binary_sensor::BinarySensor *s) { this->yaml_ready_binary_sensor_ = s; }
  void set_yaml_text_sensor(text_sensor::TextSensor *s) { this->yaml_text_sensor_ = s; }
  void dump_channel_floor_limits(uint8_t channel);

  // YAML generation accessors
  std::string get_yaml_suggestion() const { return this->yaml_last_suggestion_; }
  std::string get_yaml_climate() const { return this->yaml_last_climate_; }
  std::string get_yaml_battery() const { return this->yaml_last_battery_; }
  std::string get_yaml_temperature() const { return this->yaml_last_temperature_; }
  std::string get_yaml_floor_temperature() const { return this->yaml_last_floor_temperature_; }
  std::string get_yaml_group_climate() const { return this->yaml_last_group_climate_; }
  std::string get_yaml_group_climate_chunk(uint8_t start, uint8_t count) const;
  std::string get_yaml_climate_chunk(uint8_t start, uint8_t count) const;
  std::string get_yaml_comfort_climate_chunk(uint8_t start, uint8_t count) const;
  std::string get_yaml_battery_chunk(uint8_t start, uint8_t count) const;
  std::string get_yaml_temperature_chunk(uint8_t start, uint8_t count) const;
  std::string get_yaml_floor_temperature_chunk(uint8_t start, uint8_t count) const;
  std::string get_yaml_floor_min_temperature_chunk(uint8_t start, uint8_t count) const;
  std::string get_yaml_floor_max_temperature_chunk(uint8_t start, uint8_t count) const;
  std::string get_yaml_child_lock_chunk(uint8_t start, uint8_t count) const;
  uint8_t get_yaml_active_count() const { return (uint8_t)this->yaml_active_channels_.size(); }
  bool is_channel_grouped(uint8_t ch) const { return this->yaml_grouped_channels_.count(ch) != 0; }
  bool is_channel_child_locked(uint8_t ch) const {
    auto it = this->channels_.find(ch);
    if (it == this->channels_.end()) return false;
    return it->second.child_lock;
  }
  /// NEW: Check if channel has responded recently
  bool is_channel_online(uint8_t ch) const {
    auto it = this->channels_.find(ch);
    if (it == this->channels_.end()) return false;
    return it->second.is_online;
  }
  /// NEW: Get Modbus communication success rate percentage
  float get_comm_success_rate() const;

  // Data accessors
  float get_channel_current_temp(uint8_t channel) const;
  float get_channel_setpoint(uint8_t channel) const;
  float get_channel_floor_temp(uint8_t channel) const;
  float get_channel_floor_min_temp(uint8_t channel) const;
  float get_channel_floor_max_temp(uint8_t channel) const;
  /// NEW: Get valve position percentage (0-100)
  float get_channel_output_percent(uint8_t channel) const;
  climate::ClimateMode get_channel_mode(uint8_t channel) const;
  climate::ClimateAction get_channel_action(uint8_t channel) const;

 protected:
  // Protocol
  bool read_registers(uint8_t category, uint8_t page, uint8_t index, uint8_t count, std::vector<uint16_t> &out);
  bool write_register(uint8_t category, uint8_t page, uint8_t index, uint16_t value);
  bool write_masked_register(uint8_t category, uint8_t page, uint8_t index, uint16_t and_mask, uint16_t or_mask);
  void publish_updates();

  /// IMPROVED: Moved to .cpp with validation and clamping
  float raw_to_c(float raw) const;
  uint16_t c_to_raw(float c) const;

  /// Channel state with online tracking, output feedback, and cache flags
  struct ChannelState {
    float current_temp_c{NAN};
    float floor_temp_c{NAN};
    float floor_min_c{NAN};
    float floor_max_c{NAN};
    float setpoint_c{NAN};
    climate::ClimateMode mode{climate::CLIMATE_MODE_HEAT};
    climate::ClimateAction action{climate::CLIMATE_ACTION_OFF};
    uint8_t battery_pct{255};  // 0-100; 255=unknown
    uint8_t output_pct{0};     // NEW: Valve position 0-100%
    uint16_t primary_index{0};
    uint32_t last_seen_ms{0};  // NEW: For online detection
    bool all_tp_lost{false};
    bool has_floor_sensor{false};
    bool child_lock{false};
    bool is_online{true};      // NEW: Online status
    bool floor_limits_cached{false};  // NEW: Skip re-reading read-only values
  };

  std::map<uint8_t, ChannelState> channels_;
  std::vector<WavinZoneClimate *> single_ch_climates_;
  std::vector<WavinZoneClimate *> group_climates_;
  std::map<uint8_t, sensor::Sensor *> battery_sensors_;
  std::map<uint8_t, sensor::Sensor *> temperature_sensors_;
  std::map<uint8_t, sensor::Sensor *> floor_temperature_sensors_;
  std::map<uint8_t, sensor::Sensor *> floor_min_temperature_sensors_;
  std::map<uint8_t, sensor::Sensor *> floor_max_temperature_sensors_;
  std::map<uint8_t, sensor::Sensor *> output_sensors_;      // NEW
  std::map<uint8_t, sensor::Sensor *> comfort_setpoint_sensors_;
  std::map<uint8_t, switch_::Switch *> child_lock_switches_;
  std::map<uint8_t, binary_sensor::BinarySensor *> online_sensors_;  // NEW
  sensor::Sensor *comm_health_sensor_{nullptr};  // NEW
  binary_sensor::BinarySensor *yaml_ready_binary_sensor_{nullptr};
  text_sensor::TextSensor *yaml_text_sensor_{nullptr};

  std::string yaml_last_suggestion_{};
  std::string yaml_last_climate_{};
  std::string yaml_last_battery_{};
  std::string yaml_last_temperature_{};
  std::string yaml_last_floor_temperature_{};
  std::string yaml_last_group_climate_{};
  std::vector<std::vector<uint8_t>> yaml_group_climate_groups_;
  std::vector<uint8_t> yaml_active_channels_{};
  std::vector<uint8_t> yaml_floor_channels_{};
  std::vector<uint8_t> yaml_comfort_climate_channels_{};
  std::vector<uint8_t> yaml_child_lock_channels_{};
  std::set<uint8_t> yaml_grouped_channels_;
  std::vector<std::string> channel_friendly_names_;
  std::vector<uint8_t> active_channels_;
  std::map<uint8_t, climate::ClimateMode> desired_mode_;
  std::set<uint8_t> strict_mode_channels_;

  float temp_divisor_{10.0f};
  uint32_t last_poll_ms_{0};
  uint32_t receive_timeout_ms_{1000};
  uint32_t suspend_polling_until_{0};
  GPIOPin *tx_enable_pin_{nullptr};
  GPIOPin *flow_control_pin_{nullptr};
  uint8_t poll_channels_per_cycle_{2};
  uint8_t next_active_index_{0};
  uint8_t channel_step_[16] = {0};
  
  /// IMPROVED: Starvation prevention for urgent queue
  uint8_t consecutive_urgent_polls_{0};  // NEW
  std::vector<uint8_t> urgent_channels_{};
  bool allow_mode_writes_{true};
  bool yaml_discovery_complete_{false};  // NEW: Prevent repeated discovery
  
  /// NEW: Communication health tracking
  uint32_t total_modbus_reads_{0};
  uint32_t failed_modbus_reads_{0};
  uint32_t total_modbus_writes_{0};
  uint32_t failed_modbus_writes_{0};
  uint32_t last_comm_health_update_{0};

  uint16_t yaml_primary_present_mask_{0};
  uint16_t yaml_elem_read_mask_{0};

  // ==================== Protocol Constants ====================
  
  static constexpr uint8_t DEVICE_ADDR = 0x01;
  static constexpr uint8_t FC_READ = 0x43;
  static constexpr uint8_t FC_WRITE = 0x44;
  static constexpr uint8_t FC_WRITE_MASKED = 0x45;
  
  /// IMPROVED: Safety and performance constants
  static constexpr uint8_t MAX_CONSECUTIVE_URGENT = 3;
  static constexpr uint32_t ONLINE_TIMEOUT_MS = 300000;  // 5 minutes
  static constexpr size_t MAX_RX_BUFFER_SIZE = 64;  // Prevent buffer overflow
  static constexpr uint8_t IO_RETRY_ATTEMPTS = 2;
  static constexpr uint8_t INTER_BYTE_TIMEOUT_MS = 50;  // Modbus inter-byte detection

  // Categories
  static constexpr uint8_t CAT_CHANNELS = 0x03;
  static constexpr uint8_t CAT_ELEMENTS = 0x01;
  static constexpr uint8_t CAT_PACKED = 0x02;

  // Channel register indices
  static constexpr uint8_t CH_TIMER_EVENT = 0x00;
  static constexpr uint16_t CH_TIMER_EVENT_OUTP_ON_MASK = 0x0010;
  static constexpr uint8_t CH_PRIMARY_ELEMENT = 0x02;
  static constexpr uint16_t CH_PRIMARY_ELEMENT_ELEMENT_MASK = 0x003f;
  static constexpr uint16_t CH_PRIMARY_ELEMENT_ALL_TP_LOST_MASK = 0x0400;
  static constexpr uint8_t CH_OUTPUT_PERCENT = 0x08;  // NEW: Valve position

  // Element register indices
  static constexpr uint8_t ELEM_AIR_TEMPERATURE = 0x04;
  static constexpr uint8_t ELEM_FLOOR_TEMPERATURE = 0x05;
  static constexpr uint8_t ELEM_BATTERY_STATUS = 0x0A;  // IMPROVED: Now actively read

  // Packed register indices
  static constexpr uint8_t PACKED_MANUAL_TEMPERATURE = 0x00;
  static constexpr uint8_t PACKED_STANDBY_TEMPERATURE = 0x04;
  static constexpr uint8_t PACKED_CONFIGURATION = 0x07;
  static constexpr uint8_t PACKED_FLOOR_MIN_TEMPERATURE = 0x0A;
  static constexpr uint8_t PACKED_FLOOR_MAX_TEMPERATURE = 0x0B;
  
  // Configuration register masks
  static constexpr uint16_t PACKED_CONFIGURATION_MODE_MASK = 0x07;
  static constexpr uint16_t PACKED_CONFIGURATION_MODE_MANUAL = 0x00;
  static constexpr uint16_t PACKED_CONFIGURATION_MODE_STANDBY = 0x01;
  static constexpr uint16_t PACKED_CONFIGURATION_MODE_STANDBY_ALT = 0x04;
  static constexpr uint16_t PACKED_CONFIGURATION_PROGRAM_BIT = 0x0008;
  static constexpr uint16_t PACKED_CONFIGURATION_PROGRAM_MASK = 0x0018;
  static constexpr uint16_t PACKED_CONFIGURATION_STRICT_UNLOCK_MASK = 0x0078;
  static constexpr uint16_t PACKED_CONFIGURATION_CHILD_LOCK_MASK = 0x0800;
};

/// Child lock control switch
class WavinChildLockSwitch : public switch_::Switch {
 public:
  void set_parent(WavinAHC9000 *p) { this->parent_ = p; }
  void set_channel(uint8_t ch) { this->channel_ = ch; }
  
 protected:
  void write_state(bool state) override {
    if (this->parent_ != nullptr) {
      this->parent_->write_channel_child_lock(this->channel_, state);
    }
    this->publish_state(state);
  }
  
  WavinAHC9000 *parent_{nullptr};
  uint8_t channel_{0};
};

// ==================== Inline Implementations ====================

inline void WavinAHC9000::add_channel_battery_sensor(uint8_t ch, sensor::Sensor *s) {
  this->battery_sensors_[ch] = s;
}

inline void WavinAHC9000::add_channel_temperature_sensor(uint8_t ch, sensor::Sensor *s) {
  this->temperature_sensors_[ch] = s;
}

inline void WavinAHC9000::add_channel_comfort_setpoint_sensor(uint8_t ch, sensor::Sensor *s) {
  this->comfort_setpoint_sensors_[ch] = s;
}

inline void WavinAHC9000::add_channel_floor_temperature_sensor(uint8_t ch, sensor::Sensor *s) {
  this->floor_temperature_sensors_[ch] = s;
}

inline void WavinAHC9000::add_channel_floor_min_temperature_sensor(uint8_t ch, sensor::Sensor *s) {
  this->floor_min_temperature_sensors_[ch] = s;
}

inline void WavinAHC9000::add_channel_floor_max_temperature_sensor(uint8_t ch, sensor::Sensor *s) {
  this->floor_max_temperature_sensors_[ch] = s;
}

inline void WavinAHC9000::add_channel_output_sensor(uint8_t ch, sensor::Sensor *s) {
  this->output_sensors_[ch] = s;
}

inline void WavinAHC9000::add_channel_online_sensor(uint8_t ch, binary_sensor::BinarySensor *s) {
  this->online_sensors_[ch] = s;
}

// ==================== Zone Climate ====================

/// Per-channel or group climate entity
class WavinZoneClimate : public climate::Climate, public Component {
 public:
  void set_parent(WavinAHC9000 *p) { this->parent_ = p; }
  void set_single_channel(uint8_t ch) {
    this->single_channel_ = ch;
    this->single_channel_set_ = true;
    this->members_.clear();
  }
  void set_use_floor_temperature(bool v) { this->use_floor_temperature_ = v; }
  void set_members(const std::vector<int> &members) {
    this->members_.clear();
    for (int m : members) this->members_.push_back(static_cast<uint8_t>(m));
    this->single_channel_set_ = false;
  }

  void dump_config() override;
  void update_from_parent();

 protected:
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  WavinAHC9000 *parent_{nullptr};
  uint8_t single_channel_{0};
  bool single_channel_set_{false};
  std::vector<uint8_t> members_{};
  bool use_floor_temperature_{false};
};

}  // namespace wavin_ahc9000
}  // namespace esphome