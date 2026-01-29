#include "wavin_ahc9000.h"
#include "esphome/core/log.h"
#include "esphome/components/sensor/sensor.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace esphome {
namespace wavin_ahc9000 {

static const char *const TAG = "wavin_ahc9000";

// Simple Modbus CRC16 (0xA001 poly)
static uint16_t crc16(const uint8_t *frame, size_t len) {
  uint16_t temp = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    temp ^= frame[i];
    for (uint8_t j = 0; j < 8; j++) {
      bool flag = temp & 0x0001;
      temp >>= 1;
      if (flag) temp ^= 0xA001;
    }
  }
  return temp;
}

void WavinAHC9000::setup() { ESP_LOGCONFIG(TAG, "Wavin AHC9000 hub setup"); }
void WavinAHC9000::loop() {}

void WavinAHC9000::set_channel_friendly_name(uint8_t channel, const std::string &name) {
  if (channel < 1 || channel > 16) return;
  if (this->channel_friendly_names_.size() < 17) this->channel_friendly_names_.assign(17, std::string());
  this->channel_friendly_names_[channel] = name;
}

std::string WavinAHC9000::get_channel_friendly_name(uint8_t channel) const {
  if (channel < 1 || channel > 16) return std::string();
  if (this->channel_friendly_names_.size() < 17) return std::string();
  return this->channel_friendly_names_[channel];
}

void WavinAHC9000::update() {
  if (this->suspend_polling_until_ != 0 && millis() < this->suspend_polling_until_) {
    ESP_LOGV(TAG, "Polling suspended for %u ms more", (unsigned) (this->suspend_polling_until_ - millis()));
    return;
  }

  std::vector<uint16_t> regs;
  uint8_t urgent_processed = 0;
  
  // 1. Process urgent channels (e.g. after a write)
  while (!this->urgent_channels_.empty() && urgent_processed < this->poll_channels_per_cycle_) {
    uint8_t ch = this->urgent_channels_.front();
    this->urgent_channels_.erase(this->urgent_channels_.begin());
    uint8_t ch_page = (uint8_t) (ch - 1);
    auto &st = this->channels_[ch];

    if (this->read_registers(CAT_PACKED, ch_page, PACKED_CONFIGURATION, 1, regs) && regs.size() >= 1) {
      uint16_t raw_cfg = regs[0];
      uint16_t mode_bits = raw_cfg & PACKED_CONFIGURATION_MODE_MASK;
      bool is_off = (mode_bits == PACKED_CONFIGURATION_MODE_STANDBY) || (mode_bits == PACKED_CONFIGURATION_MODE_STANDBY_ALT);
      st.mode = is_off ? climate::CLIMATE_MODE_OFF : climate::CLIMATE_MODE_HEAT;
      st.child_lock = (raw_cfg & PACKED_CONFIGURATION_CHILD_LOCK_MASK) != 0;
      
      auto it_des = this->desired_mode_.find(ch);
      if (it_des != this->desired_mode_.end()) {
        if (it_des->second != st.mode) {
          uint16_t new_bits = (it_des->second == climate::CLIMATE_MODE_OFF) ? PACKED_CONFIGURATION_MODE_STANDBY : PACKED_CONFIGURATION_MODE_MANUAL;
          uint16_t next = (uint16_t) ((raw_cfg & ~PACKED_CONFIGURATION_MODE_MASK) | (new_bits & PACKED_CONFIGURATION_MODE_MASK));
          if (this->write_register(CAT_PACKED, ch_page, PACKED_CONFIGURATION, next)) {
            this->urgent_channels_.push_back(ch);
            this->suspend_polling_until_ = millis() + 100;
          }
        } else {
          this->desired_mode_.erase(it_des);
        }
      }
    }
    
    if (this->read_registers(CAT_PACKED, ch_page, PACKED_MANUAL_TEMPERATURE, 1, regs) && regs.size() >= 1) {
      st.setpoint_c = this->raw_to_c(regs[0]);
    }
    if (this->read_registers(CAT_PACKED, ch_page, PACKED_FLOOR_MIN_TEMPERATURE, 2, regs) && regs.size() >= 2) {
      st.floor_min_c = this->raw_to_c(regs[0]);
      st.floor_max_c = this->raw_to_c(regs[1]);
    }
    if (this->read_registers(CAT_CHANNELS, ch_page, CH_TIMER_EVENT, 1, regs) && regs.size() >= 1) {
      bool heating = (regs[0] & CH_TIMER_EVENT_OUTP_ON_MASK) != 0;
      st.action = heating ? climate::CLIMATE_ACTION_HEATING : climate::CLIMATE_ACTION_IDLE;
    }

    if (!st.all_tp_lost && st.primary_index > 0) {
      uint8_t elem_page = (uint8_t) (st.primary_index - 1);
      if (this->read_registers(CAT_ELEMENTS, elem_page, 0x00, 11, regs) && regs.size() > ELEM_AIR_TEMPERATURE) {
        st.current_temp_c = this->raw_to_c(regs[ELEM_AIR_TEMPERATURE]);
        if (regs.size() > ELEM_FLOOR_TEMPERATURE) {
          float ft = this->raw_to_c(regs[ELEM_FLOOR_TEMPERATURE]);
          if (ft > 1.0f && ft < 90.0f) { st.floor_temp_c = ft; st.has_floor_sensor = true; }
          else { st.floor_temp_c = NAN; }
        }
        // RSSI reading for Urgent Channels
        if (regs.size() > ELEM_RSSI) {
          uint16_t rssi_raw = regs[ELEM_RSSI];
          int8_t rssi_element = (int8_t)((rssi_raw >> 8) & 0xFF);
          int8_t rssi_controller = (int8_t)(rssi_raw & 0xFF);
          st.rssi_element_dbm = this->rssi_raw_to_dbm(rssi_element);
          st.rssi_controller_dbm = this->rssi_raw_to_dbm(rssi_controller);
          ESP_LOGV(TAG, "CH%u RSSI: element=%.1f dBm, controller=%.1f dBm", (unsigned) ch, st.rssi_element_dbm, st.rssi_controller_dbm);
        }
      }
    }
    urgent_processed++;
  }

  // 2. Round-robin staged reads
  if (this->active_channels_.empty()) {
    for (uint8_t ch = 1; ch <= 16; ch++) this->active_channels_.push_back(ch);
  }

  for (uint8_t i = urgent_processed; i < this->poll_channels_per_cycle_ && !this->active_channels_.empty(); i++) {
    if (this->next_active_index_ >= this->active_channels_.size()) this->next_active_index_ = 0;
    uint8_t ch_num = this->active_channels_[this->next_active_index_];
    uint8_t ch_page = (uint8_t) (ch_num - 1);
    auto &st = this->channels_[ch_num];
    uint8_t &step = this->channel_step_[ch_page];

    for (int s = 0; s < 2; s++) {
      switch (step) {
        case 0:
          if (this->read_registers(CAT_CHANNELS, ch_page, CH_PRIMARY_ELEMENT, 1, regs) && regs.size() >= 1) {
            st.primary_index = regs[0] & CH_PRIMARY_ELEMENT_ELEMENT_MASK;
            st.all_tp_lost = (regs[0] & CH_PRIMARY_ELEMENT_ALL_TP_LOST_MASK) != 0;
          }
          step = 1; break;
        case 1:
          if (this->read_registers(CAT_PACKED, ch_page, PACKED_CONFIGURATION, 1, regs) && regs.size() >= 1) {
            uint16_t mode_bits = regs[0] & PACKED_CONFIGURATION_MODE_MASK;
            st.mode = (mode_bits == PACKED_CONFIGURATION_MODE_STANDBY || mode_bits == PACKED_CONFIGURATION_MODE_STANDBY_ALT) ? climate::CLIMATE_MODE_OFF : climate::CLIMATE_MODE_HEAT;
            st.child_lock = (regs[0] & PACKED_CONFIGURATION_CHILD_LOCK_MASK) != 0;
          }
          step = 2; break;
        case 2:
          if (this->read_registers(CAT_PACKED, ch_page, PACKED_MANUAL_TEMPERATURE, 1, regs) && regs.size() >= 1) st.setpoint_c = this->raw_to_c(regs[0]);
          step = 3; break;
        case 3:
          this->read_registers(CAT_PACKED, ch_page, PACKED_FLOOR_MIN_TEMPERATURE, 2, regs);
          if (regs.size() >= 2) { st.floor_min_c = this->raw_to_c(regs[0]); st.floor_max_c = this->raw_to_c(regs[1]); }
          if (this->read_registers(CAT_CHANNELS, ch_page, CH_TIMER_EVENT, 1, regs) && regs.size() >= 1) {
            st.action = (regs[0] & CH_TIMER_EVENT_OUTP_ON_MASK) ? climate::CLIMATE_ACTION_HEATING : climate::CLIMATE_ACTION_IDLE;
          }
          step = 4; break;
        case 4:
          if (!st.all_tp_lost && st.primary_index > 0) {
            uint8_t elem_page = (uint8_t) (st.primary_index - 1);
            if (this->read_registers(CAT_ELEMENTS, elem_page, 0x00, 11, regs) && regs.size() > ELEM_AIR_TEMPERATURE) {
              st.current_temp_c = this->raw_to_c(regs[ELEM_AIR_TEMPERATURE]);
              if (regs.size() > ELEM_FLOOR_TEMPERATURE) {
                float ft = this->raw_to_c(regs[ELEM_FLOOR_TEMPERATURE]);
                if (ft > 1.0f && ft < 90.0f) { st.floor_temp_c = ft; st.has_floor_sensor = true; }
              }
              // RSSI Staged Read
              if (regs.size() > ELEM_RSSI) {
                uint16_t rssi_raw = regs[ELEM_RSSI];
                st.rssi_element_dbm = this->rssi_raw_to_dbm((int8_t)((rssi_raw >> 8) & 0xFF));
                st.rssi_controller_dbm = this->rssi_raw_to_dbm((int8_t)(rssi_raw & 0xFF));
              }
              // Publish Sensors
              if (this->temperature_sensors_.count(ch_num)) this->temperature_sensors_[ch_num]->publish_state(st.current_temp_c);
              if (this->floor_temperature_sensors_.count(ch_num) && !std::isnan(st.floor_temp_c)) this->floor_temperature_sensors_[ch_num]->publish_state(st.floor_temp_c);
              
              // RSSI Sensors Publish
              auto it_rssi_el = this->rssi_element_sensors_.find(ch_num);
              if (it_rssi_el != this->rssi_element_sensors_.end() && it_rssi_el->second && !std::isnan(st.rssi_element_dbm)) it_rssi_el->second->publish_state(st.rssi_element_dbm);
              auto it_rssi_cu = this->rssi_controller_sensors_.find(ch_num);
              if (it_rssi_cu != this->rssi_controller_sensors_.end() && it_rssi_cu->second && !std::isnan(st.rssi_controller_dbm)) it_rssi_cu->second->publish_state(st.rssi_controller_dbm);

              if (regs.size() > ELEM_BATTERY_STATUS) {
                st.battery_pct = (uint8_t)(std::min<uint16_t>(regs[ELEM_BATTERY_STATUS], 10) * 10);
                if (this->battery_sensors_.count(ch_num)) this->battery_sensors_[ch_num]->publish_state(st.battery_pct);
              }
            }
          }
          step = 0; break;
      }
    }
    this->next_active_index_ = (uint8_t) ((this->next_active_index_ + 1) % this->active_channels_.size());
  }
  this->publish_updates();
}

// YAML Suggestions & RSSI Builders
static std::string build_rssi_element_yaml_for(const WavinAHC9000 *parent, const std::vector<uint8_t> &chs) {
  std::string y;
  for (auto ch : chs) {
    std::string fname = parent->get_channel_friendly_name(ch);
    if (fname.empty()) fname = "Zone " + std::to_string((int) ch);
    y += "- platform: wavin_ahc9000\n  wavin_ahc9000_id: wavin\n  name: \"" + fname + " RSSI Element\"\n  channel: " + std::to_string((int) ch) + "\n  type: rssi_element\n";
  }
  return y;
}

static std::string build_rssi_controller_yaml_for(const WavinAHC9000 *parent, const std::vector<uint8_t> &chs) {
  std::string y;
  for (auto ch : chs) {
    std::string fname = parent->get_channel_friendly_name(ch);
    if (fname.empty()) fname = "Zone " + std::to_string((int) ch);
    y += "- platform: wavin_ahc9000\n  wavin_ahc9000_id: wavin\n  name: \"" + fname + " RSSI Controller\"\n  channel: " + std::to_string((int) ch) + "\n  type: rssi_controller\n";
  }
  return y;
}

std::string WavinAHC9000::get_yaml_rssi_element_chunk(uint8_t start, uint8_t count) const {
  if (start >= this->active_channels_.size() || count == 0) return "";
  uint8_t end = (uint8_t) std::min<size_t>(this->active_channels_.size(), (size_t) start + count);
  std::vector<uint8_t> chs(this->active_channels_.begin() + start, this->active_channels_.begin() + end);
  return build_rssi_element_yaml_for(this, chs);
}

std::string WavinAHC9000::get_yaml_rssi_controller_chunk(uint8_t start, uint8_t count) const {
  if (start >= this->active_channels_.size() || count == 0) return "";
  uint8_t end = (uint8_t) std::min<size_t>(this->active_channels_.size(), (size_t) start + count);
  std::vector<uint8_t> chs(this->active_channels_.begin() + start, this->active_channels_.begin() + end);
  return build_rssi_controller_yaml_for(this, chs);
}

void WavinAHC9000::generate_yaml_suggestion() {
  std::vector<uint16_t> regs;
  for (uint8_t ch = 1; ch <= 16; ch++) {
    uint8_t page = (uint8_t) (ch - 1);
    if (this->read_registers(CAT_CHANNELS, page, CH_PRIMARY_ELEMENT, 1, regs) && regs.size() >= 1) {
      uint16_t primary_index = regs[0] & CH_PRIMARY_ELEMENT_ELEMENT_MASK;
      if (primary_index > 0 && !(regs[0] & CH_PRIMARY_ELEMENT_ALL_TP_LOST_MASK)) {
        auto &st = this->channels_[ch];
        st.primary_index = primary_index;
        uint8_t elem_page = (uint8_t) (primary_index - 1);
        if (this->read_registers(CAT_ELEMENTS, elem_page, 0x00, 11, regs) && regs.size() > ELEM_RSSI) {
          uint16_t rssi_raw = regs[ELEM_RSSI];
          st.rssi_element_dbm = this->rssi_raw_to_dbm((int8_t)((rssi_raw >> 8) & 0xFF));
          st.rssi_controller_dbm = this->rssi_raw_to_dbm((int8_t)(rssi_raw & 0xFF));
        }
      }
    }
  }
}

// IO & Write Helpers (Modbus implementation)
bool WavinAHC9000::read_registers(uint8_t category, uint8_t page, uint8_t index, uint8_t count, std::vector<uint16_t> &out) {
  for (uint8_t attempt = 0; attempt < IO_RETRY_ATTEMPTS; attempt++) {
    uint8_t msg[8] = {DEVICE_ADDR, FC_READ, category, index, page, count};
    uint16_t crc = crc16(msg, 6);
    msg[6] = crc & 0xFF; msg[7] = crc >> 8;
    if (this->flow_control_pin_) this->flow_control_pin_->digital_write(true);
    this->write_array(msg, 8); this->flush();
    delayMicroseconds(250);
    if (this->flow_control_pin_) this->flow_control_pin_->digital_write(false);
    
    std::vector<uint8_t> buf;
    uint32_t start = millis();
    while (millis() - start < this->receive_timeout_ms_) {
      if (this->available()) {
        buf.push_back(this->read());
        if (buf.size() >= 5 && buf.size() == (size_t)(buf[2] + 5)) {
          if (crc16(buf.data(), buf.size()) == 0) {
            out.clear();
            for (uint8_t i = 0; i + 1 < buf[2]; i += 2) out.push_back((buf[3+i] << 8) | buf[3+i+1]);
            return true;
          }
        }
      }
    }
  }
  return false;
}

bool WavinAHC9000::write_register(uint8_t category, uint8_t page, uint8_t index, uint16_t value) {
  for (uint8_t attempt = 0; attempt < IO_RETRY_ATTEMPTS; attempt++) {
    uint8_t msg[10] = {DEVICE_ADDR, FC_WRITE, category, index, page, 1, (uint8_t)(value >> 8), (uint8_t)(value & 0xFF)};
    uint16_t crc = crc16(msg, 8);
    msg[8] = crc & 0xFF; msg[9] = crc >> 8;
    if (this->flow_control_pin_) this->flow_control_pin_->digital_write(true);
    this->write_array(msg, 10); this->flush();
    delayMicroseconds(250);
    if (this->flow_control_pin_) this->flow_control_pin_->digital_write(false);
    
    std::vector<uint8_t> buf;
    uint32_t start = millis();
    while (millis() - start < this->receive_timeout_ms_) {
      if (this->available()) {
        buf.push_back(this->read());
        if (buf.size() >= 5 && buf.size() == (size_t)(buf[2] + 5)) return crc16(buf.data(), buf.size()) == 0;
      }
    }
  }
  return false;
}

void WavinAHC9000::publish_updates() {
  for (auto &kv : this->rssi_element_sensors_) {
    if (kv.second && this->channels_.count(kv.first)) {
      float v = this->channels_[kv.first].rssi_element_dbm;
      if (!std::isnan(v)) kv.second->publish_state(v);
    }
  }
  for (auto &kv : this->rssi_controller_sensors_) {
    if (kv.second && this->channels_.count(kv.first)) {
      float v = this->channels_[kv.first].rssi_controller_dbm;
      if (!std::isnan(v)) kv.second->publish_state(v);
    }
  }
}

float WavinAHC9000::get_channel_rssi_element(uint8_t channel) const {
  auto it = this->channels_.find(channel);
  return it == this->channels_.end() ? NAN : it->second.rssi_element_dbm;
}
float WavinAHC9000::get_channel_rssi_controller(uint8_t channel) const {
  auto it = this->channels_.find(channel);
  return it == this->channels_.end() ? NAN : it->second.rssi_controller_dbm;
}

} // namespace wavin_ahc9000
} // namespace esphome
