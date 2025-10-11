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
  // Use direct indexing for better performance
  for (size_t i = OFFSET_LENGTH; i < this->data_.size(); ++i)
    cs -= this->data_[i];
  return cs & 0xFF;
}


}  // namespace midea
}  // namespace esphome
