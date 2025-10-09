#include "frame.h"

namespace esphome {
namespace midea {

void Frame::setData(const FrameData &data) {
  this->trimData_();
  this->appendData_(data);
  this->data_[OFFSET_LENGTH] = this->data_.size();
  this->data_[OFFSET_SYNC] = this->data_[OFFSET_LENGTH] ^ this->data_[OFFSET_APPTYPE];
  this->appendCS_();
}

uint8_t Frame::calcCS_() const {
  if (this->data_.size() <= OFFSET_LENGTH)
    return -1;
  
  uint16_t cs = 0;
  for (auto it = this->data_.begin() + OFFSET_LENGTH; it != this->data_.end(); ++it)
    cs -= *it;
  return cs & 0xFF;
}

static char u4hex(uint8_t num) { return num + ((num < 10) ? '0' : ('A' - 10)); }

std::string Frame::toString() const {
  std::string ret;
  char buf[4];
  buf[2] = ' ';
  buf[3] = '\0';
  ret.reserve(3 * this->size());
  for (const uint8_t data : this->data_) {
    buf[0] = u4hex(data / 16);
    buf[1] = u4hex(data % 16);
    ret += buf;
  }
  return ret;
}

}  // namespace midea
}  // namespace esphome
