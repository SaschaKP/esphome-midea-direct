#pragma once
#include <vector>
#include <string>
#include <iterator>
#include "frame_data.h"

namespace esphome {
namespace midea {

class Frame {
 public:
  Frame() = default;
  Frame(uint8_t appliance, uint8_t protocol, uint8_t type, const FrameData &data)
  : data_({START_BYTE, 0x00, appliance, 0x00, 0x00, 0x00, 0x00, 0x00, protocol, type}) {
    this->setData(data);
  }
  FrameData getData() const { return FrameData(this->data_.data() + OFFSET_DATA, this->len_() - OFFSET_DATA); }
  void setData(const FrameData &data);
  bool isValid() const { return !this->calcCS_(); }

  const uint8_t *data() const { return this->data_.data(); }
  uint8_t size() const { return this->data_.size(); }
  void setType(uint8_t value) { this->data_[OFFSET_TYPE] = value; }
  bool hasType(uint8_t value) const { return this->data_[OFFSET_TYPE] == value; }
  void setProtocol(uint8_t value) { this->data_[OFFSET_PROTOCOL] = value; }
  uint8_t getProtocol() const { return this->data_[OFFSET_PROTOCOL]; }
  std::string toString() const;

 protected:
  std::vector<uint8_t> data_;
  void trimData_() { this->data_.erase(this->data_.begin() + OFFSET_DATA, this->data_.end()); }
  void appendData_(const FrameData &data) { std::copy(data.data(), data.data() + data.size(), std::back_inserter(this->data_)); }
  uint8_t len_() const { return this->data_[OFFSET_LENGTH]; }
  void appendCS_() { this->data_.push_back(this->calcCS_()); }
  uint8_t calcCS_() const;
  static const uint8_t START_BYTE = 0xAA;
  static const uint8_t OFFSET_START = 0;
  static const uint8_t OFFSET_LENGTH = 1;
  static const uint8_t OFFSET_APPTYPE = 2;
  static const uint8_t OFFSET_SYNC = 3;
  static const uint8_t OFFSET_PROTOCOL = 8;
  static const uint8_t OFFSET_TYPE = 9;
  static const uint8_t OFFSET_DATA = 10;
};

}  // namespace midea
}  // namespace esphome
