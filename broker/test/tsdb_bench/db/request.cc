#include "request.hh"

/********************************************************************************
 *
 *     expandable_buffer
 *
 ********************************************************************************/

constexpr size_t expandable_unit = 65536;

expandable_buffer::expandable_buffer()
    : _buffer(nullptr), _size(0), _allocated(0) {}

expandable_buffer::~expandable_buffer() {
  if (_buffer) {
    free(_buffer);
  }
}

void expandable_buffer::reserve(size_t byte_size) {
  if (_size + byte_size > _allocated) {
    _allocated = (_size + byte_size + expandable_unit) / expandable_unit *
                 expandable_unit;
    uint8_t* newbuff = nullptr;
    if (!_buffer) {
      newbuff = static_cast<uint8_t*>(malloc(_allocated));
    } else {
      newbuff = static_cast<uint8_t*>(realloc(_buffer, _allocated));
    }
    if (!newbuff) {
      throw std::runtime_error(
          fmt::format("unable to allocate {} bytes", _allocated));
    }
    _buffer = newbuff;
  }
}
void expandable_buffer::append(const void* data, size_t byte_size) {
  reserve(byte_size);
  memcpy(_buffer + _size, data, byte_size);
  _size += byte_size;
}

void expandable_buffer::append8(uint8_t data) {
  reserve(1);
  *(_buffer + _size++) = data;
}

void expandable_buffer::hton_append16(uint16_t data) {
  reserve(2);
  *(_buffer + _size++) = data >> 8;
  *(_buffer + _size++) = data & 0x0F;
}

void expandable_buffer::hton_append32(uint32_t data) {
  reserve(4);
  *(_buffer + _size++) = data >> 24;
  *(_buffer + _size++) = (data >> 16) & 0x0F;
  *(_buffer + _size++) = (data >> 8) & 0x0F;
  *(_buffer + _size++) = data & 0x0F;
}

void expandable_buffer::hton_append64(uint64_t data) {
  reserve(8);
  *(_buffer + _size++) = data >> 56;
  *(_buffer + _size++) = (data >> 48) & 0x0F;
  *(_buffer + _size++) = (data >> 40) & 0x0F;
  *(_buffer + _size++) = (data >> 32) & 0x0F;
  *(_buffer + _size++) = (data >> 24) & 0x0F;
  *(_buffer + _size++) = (data >> 16) & 0x0F;
  *(_buffer + _size++) = (data >> 8) & 0x0F;
  *(_buffer + _size++) = data & 0x0F;
}

/********************************************************************************
 *
 *     request_base
 *
 ********************************************************************************/

void request_base::call_callback(const std::error_code& err,
                                 const std::string& err_detail) {
  bool desired = false;
  if (_callback_called.compare_exchange_strong(desired, true)) {
    _callback(err, err_detail, shared_from_this());
  }
}

void request_base::dump(std::ostream& s) const {
  s << _type << " this:" << this;
}

std::ostream& operator<<(std::ostream& s, const request_base& req) {
  (&req)->dump(s);
  return s;
}

#define CASE_REQUEST_STR(val)             \
  case request_base::e_request_type::val: \
    s << #val;                            \
    break;
std::ostream& operator<<(std::ostream& s,
                         const request_base::e_request_type& req_type) {
  switch (req_type) {
    CASE_REQUEST_STR(simple_no_result_request);
    CASE_REQUEST_STR(load_request);
    CASE_REQUEST_STR(statement_request);
    default:
      s << "unknown value:" << (int)req_type;
  }
  return s;
}

void no_result_request::dump(std::ostream& s) const {
  request_base::dump(s);
  s << ' ' << _request;
}
