#pragma once
#include <vector>
#include <cstdlib>

namespace esphome {
namespace midea {

class FrameData {
 public:
  FrameData() = delete;
  FrameData(std::vector<uint8_t>::const_iterator begin, std::vector<uint8_t>::const_iterator end) : data_(begin, end) {}
  FrameData(const uint8_t *data, uint8_t size) : data_(data, data + size) {}
  FrameData(std::initializer_list<uint8_t> list) : data_(list) {}
  FrameData(uint8_t size) : data_(size, 0) {}
  template<typename T> T to() { return std::move(*this); }
  const uint8_t *data() const { return this->data_.data(); }
  uint8_t size() const { return this->data_.size(); }
  bool hasID(uint8_t value) const { return this->data_[0] == value; }
  bool hasStatus() const { return this->hasID(0xC0); }
  bool hasPowerInfo() const { return this->hasID(0xC1); }
  void appendCRC() { this->data_.push_back(this->calcCRC_()); }
  void updateCRC() {
    this->data_.pop_back();
    this->appendCRC();
  }
  bool hasValidCRC() const { return !this->calcCRC_(); }
 protected:
  std::vector<uint8_t> data_;
  static uint8_t id_;
  static uint8_t getID_() { return FrameData::id_++; }
  static uint8_t getRandom_() { return static_cast<uint8_t>(rand() & 0xFF); }
  uint8_t calcCRC_() const;
  uint8_t getValue_(uint8_t idx, uint8_t mask = 255, uint8_t shift = 0) const;
  void setValue_(uint8_t idx, uint8_t value, uint8_t mask = 255, uint8_t shift = 0) {
    this->data_[idx] &= ~(mask << shift);
    this->data_[idx] |= (value << shift);
  }
  void setMask_(uint8_t idx, bool state, uint8_t mask = 255) { this->setValue_(idx, state ? mask : 0, mask); }
};

class NetworkNotifyData : public FrameData {
 public:
  NetworkNotifyData() : FrameData({0x01, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}) {}
  void setConnected(bool state) { this->setMask_(8, !state, 1); }
  void setSignalStrength(uint8_t value) { this->setValue_(2, value); }
  void setIP(uint8_t ipbyte1, uint8_t ipbyte2, uint8_t ipbyte3, uint8_t ipbyte4);
};

}  // namespace midea
}  // namespace esphome
