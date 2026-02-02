// ADD THESE NEW METHOD IMPLEMENTATIONS after the existing code (before the final namespace closing)

// ==================== NEW: Temperature Conversion with Validation ====================
float WavinAHC9000::raw_to_c(float raw) const {
  float temp = raw / this->temp_divisor_;
  
  // Sanity check: detect corrupt/uninitialized/default values
  // Wavin thermostats operate in ~5-35°C heating range
  // Floor sensors can read wider, but -40 to +100°C covers all cases
  if (temp < -40.0f || temp > 100.0f) {
    ESP_LOGW(TAG, "Implausible temperature: %.1f°C (raw=%.0f, divisor=%.1f)", temp, raw, this->temp_divisor_);
    return NAN;
  }
  
  return temp;
}

uint16_t WavinAHC9000::c_to_raw(float c) const {
  if (std::isnan(c)) {
    ESP_LOGW(TAG, "Attempted to convert NaN temperature to raw");
    return 0;
  }
  
  // Clamp to Wavin controller operating range (5-35°C for setpoints)
  float clamped = c;
  if (c < 5.0f) {
    ESP_LOGW(TAG, "Setpoint %.1f°C below minimum, clamping to 5.0°C", c);
    clamped = 5.0f;
  } else if (c > 35.0f) {
    ESP_LOGW(TAG, "Setpoint %.1f°C above maximum, clamping to 35.0°C", c);
    clamped = 35.0f;
  }
  
  return static_cast<uint16_t>(clamped * this->temp_divisor_ + 0.5f);
}

// ==================== NEW: Data Accessors ====================
float WavinAHC9000::get_channel_output_percent(uint8_t channel) const {
  auto it = this->channels_.find(channel);
  if (it == this->channels_.end()) return NAN;
  return (float)it->second.output_pct;
}

float WavinAHC9000::get_comm_success_rate() const {
  uint32_t total = this->total_modbus_reads_ + this->total_modbus_writes_;
  if (total == 0) return 100.0f;
  uint32_t failed = this->failed_modbus_reads_ + this->failed_modbus_writes_;
  return (float)(total - failed) / total * 100.0f;
}

// ==================== IMPROVED: update() with New Features ====================
// REPLACE THE EXISTING update() METHOD WITH THIS VERSION:
// (This adds starvation prevention, online detection, and comm health tracking)

void WavinAHC9000::update() {
  // If polling is temporarily suspended (after a write), skip until window expires
  if (this->suspend_polling_until_ != 0 && millis() < this->suspend_polling_until_) {
    ESP_LOGV(TAG, "Polling suspended for %u ms more", (unsigned) (this->suspend_polling_until_ - millis()));
    return;
  }

  // IMPROVED: Urgent queue starvation prevention
  // Allow max 3 consecutive urgent polls before forcing normal cycle
  std::vector<uint16_t> regs;
  uint8_t urgent_processed = 0;
  
  if (!this->urgent_channels_.empty() && this->consecutive_urgent_polls_ < MAX_CONSECUTIVE_URGENT) {
    uint8_t ch = this->urgent_channels_.front();
    this->urgent_channels_.erase(this->urgent_channels_.begin());
    this->consecutive_urgent_polls_++;
    
    ESP_LOGD(TAG, "Urgent poll channel %u (consecutive=%u, queue_size=%zu)", 
             ch, this->consecutive_urgent_polls_, this->urgent_channels_.size());
    
    uint8_t ch_page = (uint8_t) (ch - 1);
    auto &st = this->channels_[ch];
    
    // Perform a compact refresh sequence for the channel
    if (this->read_registers(CAT_PACKED, ch_page, PACKED_CONFIGURATION, 1, regs) && regs.size() >= 1) {
      uint16_t raw_cfg = regs[0];
      uint16_t mode_bits = raw_cfg & PACKED_CONFIGURATION_MODE_MASK;
      bool is_off = (mode_bits == PACKED_CONFIGURATION_MODE_STANDBY) || (mode_bits == PACKED_CONFIGURATION_MODE_STANDBY_ALT);
      st.mode = is_off ? climate::CLIMATE_MODE_OFF : climate::CLIMATE_MODE_HEAT;
      st.child_lock = (raw_cfg & PACKED_CONFIGURATION_CHILD_LOCK_MASK) != 0;
      
      // NEW: Mark channel as seen (online detection)
      st.last_seen_ms = millis();
      if (!st.is_online) {
        st.is_online = true;
        auto it_online = this->online_sensors_.find(ch);
        if (it_online != this->online_sensors_.end() && it_online->second != nullptr) {
          it_online->second->publish_state(true);
        }
        ESP_LOGI(TAG, "Channel %u came online", ch);
      }
      
      ESP_LOGD(TAG, "CH%u cfg=0x%04X mode=%s child_lock=%s", (unsigned) ch, (unsigned) raw_cfg, 
               is_off ? "OFF" : "HEAT", st.child_lock?"Y":"N");
      
      // Reconcile desired mode if pending and mismatch
      auto it_des = this->desired_mode_.find(ch);
      if (it_des != this->desired_mode_.end()) {
        auto want = it_des->second;
        if (want != st.mode) {
          uint16_t current = raw_cfg;
          uint16_t new_bits = (want == climate::CLIMATE_MODE_OFF) ? PACKED_CONFIGURATION_MODE_STANDBY : PACKED_CONFIGURATION_MODE_MANUAL;
          uint16_t next = (uint16_t) ((current & ~PACKED_CONFIGURATION_MODE_MASK) | (new_bits & PACKED_CONFIGURATION_MODE_MASK));
          ESP_LOGW(TAG, "Reconciling mode for ch=%u cur=0x%04X next=0x%04X", (unsigned) ch, (unsigned) current, (unsigned) next);
          if (this->write_register(CAT_PACKED, ch_page, PACKED_CONFIGURATION, next)) {
            this->urgent_channels_.push_back(ch);
            // FIXED: Extend suspension window instead of replacing
            uint32_t new_suspend_until = millis() + 100;
            if (new_suspend_until > this->suspend_polling_until_) {
              this->suspend_polling_until_ = new_suspend_until;
            }
          }
        } else {
          this->desired_mode_.erase(it_des);
        }
      }
    }
    
    if (this->read_registers(CAT_PACKED, ch_page, PACKED_MANUAL_TEMPERATURE, 1, regs) && regs.size() >= 1) {
      st.setpoint_c = this->raw_to_c(regs[0]);
    }
    
    // Read floor min/max (read-only) combined in one request
    if (this->read_registers(CAT_PACKED, ch_page, PACKED_FLOOR_MIN_TEMPERATURE, 2, regs) && regs.size() >= 2) {
      st.floor_min_c = this->raw_to_c(regs[0]);
      st.floor_max_c = this->raw_to_c(regs[1]);
      st.floor_limits_cached = true;
    }
    
    if (this->read_registers(CAT_CHANNELS, ch_page, CH_TIMER_EVENT, 1, regs) && regs.size() >= 1) {
      bool heating = (regs[0] & CH_TIMER_EVENT_OUTP_ON_MASK) != 0;
      st.action = heating ? climate::CLIMATE_ACTION_HEATING : climate::CLIMATE_ACTION_IDLE;
    }
    
    // NEW: Read output/valve position
    if (this->read_registers(CAT_CHANNELS, ch_page, CH_OUTPUT_PERCENT, 1, regs) && regs.size() >= 1) {
      uint8_t output = (uint8_t)(regs[0] & 0xFF);  // 0-100%
      st.output_pct = output;
      
      auto it_out = this->output_sensors_.find(ch);
      if (it_out != this->output_sensors_.end() && it_out->second != nullptr) {
        it_out->second->publish_state((float)output);
      }
    }
    
    if (!st.all_tp_lost && st.primary_index > 0) {
      uint8_t elem_page = (uint8_t) (st.primary_index - 1);
      if (this->read_registers(CAT_ELEMENTS, elem_page, 0x00, 11, regs) && regs.size() > ELEM_AIR_TEMPERATURE) {
        st.current_temp_c = this->raw_to_c(regs[ELEM_AIR_TEMPERATURE]);
        this->yaml_elem_read_mask_ |= (1u << (ch - 1));
        if (regs.size() > ELEM_FLOOR_TEMPERATURE) {
          float ft = this->raw_to_c(regs[ELEM_FLOOR_TEMPERATURE]);
          if (ft > 1.0f && ft < 90.0f) {
            st.floor_temp_c = ft;
            st.has_floor_sensor = true;
          } else {
            st.floor_temp_c = NAN;
          }
        }
        
        // NEW: Read battery status actively
        if (regs.size() > ELEM_BATTERY_STATUS) {
          uint16_t raw_batt = regs[ELEM_BATTERY_STATUS];
          if (raw_batt <= 100) {
            st.battery_pct = (uint8_t)raw_batt;
          } else if (raw_batt <= 255) {
            st.battery_pct = (uint8_t)((raw_batt * 100) / 255);
          } else {
            st.battery_pct = 255;
          }
          
          auto it_batt = this->battery_sensors_.find(ch);
          if (it_batt != this->battery_sensors_.end() && it_batt->second != nullptr) {
            if (st.battery_pct <= 100) {
              it_batt->second->publish_state((float)st.battery_pct);
            }
          }
        }
      }
    }
    
    urgent_processed++;
    this->publish_updates();
    return;
  }

  // Reset consecutive counter when doing normal polling
  this->consecutive_urgent_polls_ = 0;

  // Round-robin staged reads across active channels
  if (this->active_channels_.empty()) {
    this->active_channels_.reserve(16);
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
        case 0: {
          if (this->read_registers(CAT_CHANNELS, ch_page, CH_PRIMARY_ELEMENT, 1, regs) && regs.size() >= 1) {
            uint16_t v = regs[0];
            st.primary_index = v & CH_PRIMARY_ELEMENT_ELEMENT_MASK;
            st.all_tp_lost = (v & CH_PRIMARY_ELEMENT_ALL_TP_LOST_MASK) != 0;
            
            // NEW: Mark as seen
            st.last_seen_ms = millis();
            if (!st.is_online) {
              st.is_online = true;
              auto it_online = this->online_sensors_.find(ch_num);
              if (it_online != this->online_sensors_.end() && it_online->second != nullptr) {
                it_online->second->publish_state(true);
              }
            }
            
            ESP_LOGD(TAG, "CH%u primary elem=%u lost=%s", ch_num, (unsigned) st.primary_index, st.all_tp_lost ? "Y" : "N");
            if (st.primary_index > 0 && !st.all_tp_lost) this->yaml_primary_present_mask_ |= (1u << (ch_num - 1));
          } else {
            ESP_LOGW(TAG, "CH%u: primary element read failed", ch_num);
          }
          step = 1;
          break;
        }
        case 1: {
          if (this->read_registers(CAT_PACKED, ch_page, PACKED_CONFIGURATION, 1, regs) && regs.size() >= 1) {
            uint16_t raw_cfg = regs[0];
            uint16_t mode_bits = raw_cfg & PACKED_CONFIGURATION_MODE_MASK;
            bool is_off = (mode_bits == PACKED_CONFIGURATION_MODE_STANDBY) || (mode_bits == PACKED_CONFIGURATION_MODE_STANDBY_ALT);
            st.mode = is_off ? climate::CLIMATE_MODE_OFF : climate::CLIMATE_MODE_HEAT;
            st.child_lock = (raw_cfg & PACKED_CONFIGURATION_CHILD_LOCK_MASK) != 0;
            ESP_LOGD(TAG, "CH%u cfg=0x%04X mode=%s child_lock=%s", ch_num, (unsigned) raw_cfg, is_off ? "OFF" : "HEAT", st.child_lock?"Y":"N");
          } else {
            ESP_LOGW(TAG, "CH%u: mode read failed", ch_num);
          }
          step = 2;
          break;
        }
        case 2: {
          if (this->read_registers(CAT_PACKED, ch_page, PACKED_MANUAL_TEMPERATURE, 1, regs) && regs.size() >= 1) {
            st.setpoint_c = this->raw_to_c(regs[0]);
            ESP_LOGD(TAG, "CH%u setpoint=%.1fC", ch_num, st.setpoint_c);
          } else {
            ESP_LOGW(TAG, "CH%u: setpoint read failed", ch_num);
          }
          step = 3;
          break;
        }
        case 3: {
          if (this->read_registers(CAT_PACKED, ch_page, PACKED_FLOOR_MIN_TEMPERATURE, 2, regs) && regs.size() >= 2) {
            st.floor_min_c = this->raw_to_c(regs[0]);
            st.floor_max_c = this->raw_to_c(regs[1]);
            st.floor_limits_cached = true;
          }
          if (this->read_registers(CAT_CHANNELS, ch_page, CH_TIMER_EVENT, 1, regs) && regs.size() >= 1) {
            bool heating = (regs[0] & CH_TIMER_EVENT_OUTP_ON_MASK) != 0;
            st.action = heating ? climate::CLIMATE_ACTION_HEATING : climate::CLIMATE_ACTION_IDLE;
            ESP_LOGD(TAG, "CH%u action=%s", ch_num, heating ? "HEATING" : "IDLE");
          } else {
            ESP_LOGW(TAG, "CH%u: action read failed", ch_num);
          }
          step = 4;
          break;
        }
        case 4: {
          if (!st.all_tp_lost && st.primary_index > 0) {
            uint8_t elem_page = (uint8_t) (st.primary_index - 1);
            if (this->read_registers(CAT_ELEMENTS, elem_page, 0x00, 11, regs) && regs.size() > ELEM_AIR_TEMPERATURE) {
              st.current_temp_c = this->raw_to_c(regs[ELEM_AIR_TEMPERATURE]);
              this->yaml_elem_read_mask_ |= (1u << (ch_num - 1));
              
              // NEW: Mark as seen during element read
              st.last_seen_ms = millis();
              if (!st.is_online) {
                st.is_online = true;
                auto it_online = this->online_sensors_.find(ch_num);
                if (it_online != this->online_sensors_.end() && it_online->second != nullptr) {
                  it_online->second->publish_state(true);
                }
                ESP_LOGI(TAG, "Channel %u came online", ch_num);
              }
              
              if (regs.size() > ELEM_FLOOR_TEMPERATURE) {
                float ft = this->raw_to_c(regs[ELEM_FLOOR_TEMPERATURE]);
                if (ft > 1.0f && ft < 90.0f) {
                  st.floor_temp_c = ft;
                  st.has_floor_sensor = true;
                } else {
                  st.floor_temp_c = NAN;
                }
              }
              ESP_LOGD(TAG, "CH%u current=%.1fC", ch_num, st.current_temp_c);
              
              // Publish to per-channel temperature sensor
              auto it_t = this->temperature_sensors_.find(ch_num);
              if (it_t != this->temperature_sensors_.end() && it_t->second != nullptr && !std::isnan(st.current_temp_c)) {
                it_t->second->publish_state(st.current_temp_c);
              }
              
              // Floor sensor publish
              auto it_ft = this->floor_temperature_sensors_.find(ch_num);
              if (it_ft != this->floor_temperature_sensors_.end() && it_ft->second != nullptr && !std::isnan(st.floor_temp_c)) {
                it_ft->second->publish_state(st.floor_temp_c);
              }
              
              // NEW: Battery status
              if (regs.size() > ELEM_BATTERY_STATUS) {
                uint16_t raw = regs[ELEM_BATTERY_STATUS];
                uint8_t steps = (raw > 10) ? 10 : (uint8_t) raw;
                uint8_t pct = (uint8_t) (steps * 10);
                st.battery_pct = pct;
                auto it = this->battery_sensors_.find(ch_num);
                if (it != this->battery_sensors_.end() && it->second != nullptr) {
                  it->second->publish_state((float) pct);
                }
              }
            } else {
              ESP_LOGW(TAG, "CH%u: element temp read failed", ch_num);
            }
          } else {
            st.current_temp_c = NAN;
          }
          step = 0;
          break;
        }
      }
    }

    this->next_active_index_ = (uint8_t) ((this->next_active_index_ + 1) % this->active_channels_.size());
  }

  // NEW: Update channel online/offline status
  for (auto &pair : this->channels_) {
    uint8_t ch = pair.first;
    ChannelState &st = pair.second;
    
    if (st.last_seen_ms > 0 && st.is_online && (millis() - st.last_seen_ms) > ONLINE_TIMEOUT_MS) {
      st.is_online = false;
      auto it_online = this->online_sensors_.find(ch);
      if (it_online != this->online_sensors_.end() && it_online->second != nullptr) {
        it_online->second->publish_state(false);
      }
      ESP_LOGW(TAG, "Channel %u went offline (no response for %u seconds)", ch, ONLINE_TIMEOUT_MS / 1000);
    }
  }

  // NEW: Publish communication health every 60 seconds
  if (this->comm_health_sensor_ != nullptr && (millis() - this->last_comm_health_update_) > 60000) {
    float success_rate = this->get_comm_success_rate();
    this->comm_health_sensor_->publish_state(success_rate);
    this->last_comm_health_update_ = millis();
    
    ESP_LOGD(TAG, "Modbus health: %.1f%% (reads=%u/%u, writes=%u/%u)", 
             success_rate,
             this->total_modbus_reads_ - this->failed_modbus_reads_, this->total_modbus_reads_,
             this->total_modbus_writes_ - this->failed_modbus_writes_, this->total_modbus_writes_);
  }

  // publish once per cycle
  this->publish_updates();
}

// ==================== IMPROVED: refresh_channel_now() with Duplicate Detection ====================
// REPLACE THE EXISTING refresh_channel_now() WITH THIS VERSION:

void WavinAHC9000::refresh_channel_now(uint8_t channel) {
  if (channel < 1 || channel > 16) return;
  
  // FIXED: Check for duplicates to avoid queue buildup
  for (const auto &ch : this->urgent_channels_) {
    if (ch == channel) {
      ESP_LOGV(TAG, "Channel %u already in urgent queue, skipping duplicate", channel);
      return;
    }
  }
  
  this->urgent_channels_.push_back(channel);
  ESP_LOGD(TAG, "Channel %u added to urgent queue (size=%zu)", channel, this->urgent_channels_.size());
}

// ==================== IMPROVED: write_register() with Stats Tracking ====================
// ADD THIS TO THE EXISTING write_register() METHOD - at the start add:
// LOCATE: bool WavinAHC9000::write_register()
// ADD RIGHT AFTER FUNCTION STARTS (after the for loop declaration):

// Track communication stats
this->total_modbus_writes_++;

// AND BEFORE FINAL return false; ADD:
// this->failed_modbus_writes_++;

// ==================== IMPROVED: write_masked_register() with Stats Tracking ====================
// LOCATE: bool WavinAHC9000::write_masked_register()
// ADD RIGHT AFTER FUNCTION STARTS:

// Track communication stats
this->total_modbus_writes_++;

// AND BEFORE FINAL return false; ADD:
// this->failed_modbus_writes_++;

// ==================== IMPROVED: read_registers() with Stats Tracking ====================
// LOCATE: bool WavinAHC9000::read_registers()
// ADD RIGHT AFTER FUNCTION STARTS:

// Track communication stats
this->total_modbus_reads_++;

// AND BEFORE FINAL return false; ADD:
// this->failed_modbus_reads_++;

// ==================== IMPROVED: write_channel_setpoint() with Race Fix ====================
// LOCATE: void WavinAHC9000::write_channel_setpoint()
// REPLACE THIS LINE:
//   this->suspend_polling_until_ = millis() + 100;
// WITH THIS:
//   uint32_t new_suspend_until = millis() + 100;
//   if (new_suspend_until > this->suspend_polling_until_) {
//     this->suspend_polling_until_ = new_suspend_until;
//   }

// ==================== IMPROVED: write_channel_mode() with Race Fix ====================
// LOCATE: void WavinAHC9000::write_channel_mode()
// REPLACE BOTH OCCURRENCES OF:
//   this->suspend_polling_until_ = millis() + 100;
// WITH:
//   uint32_t new_suspend_until = millis() + 100;
//   if (new_suspend_until > this->suspend_polling_until_) {
//     this->suspend_polling_until_ = new_suspend_until;
//   }

// ==================== IMPROVED: write_channel_child_lock() with Race Fix ====================
// LOCATE: void WavinAHC9000::write_channel_child_lock()
// REPLACE:
//   this->suspend_polling_until_ = millis() + 100;
// WITH:
//   uint32_t new_suspend_until = millis() + 100;
//   if (new_suspend_until > this->suspend_polling_until_) {
//     this->suspend_polling_until_ = new_suspend_until;
//   }

// ==================== IMPROVED: write_channel_floor_min_temperature() with Race Fix ====================
// LOCATE: void WavinAHC9000::write_channel_floor_min_temperature()
// REPLACE:
//   this->suspend_polling_until_ = millis() + 100;
// WITH:
//   uint32_t new_suspend_until = millis() + 100;
//   if (new_suspend_until > this->suspend_polling_until_) {
//     this->suspend_polling_until_ = new_suspend_until;
//   }

// ==================== IMPROVED: write_channel_floor_max_temperature() with Race Fix ====================
// LOCATE: void WavinAHC9000::write_channel_floor_max_temperature()
// REPLACE:
//   this->suspend_polling_until_ = millis() + 100;
// WITH:
//   uint32_t new_suspend_until = millis() + 100;
//   if (new_suspend_until > this->suspend_polling_until_) {
//     this->suspend_polling_until_ = new_suspend_until;
//   }

// ==================== IMPROVED: normalize_channel_config() with Race Fix ====================
// LOCATE: void WavinAHC9000::normalize_channel_config()
// REPLACE:
//   this->suspend_polling_until_ = millis() + 100;
// WITH:
//   uint32_t new_suspend_until = millis() + 100;
//   if (new_suspend_until > this->suspend_polling_until_) {
//     this->suspend_polling_until_ = new_suspend_until;
//   }

}  // namespace wavin_ahc9000
}  // namespace esphome