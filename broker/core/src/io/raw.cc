/**
 * Copyright 2011-2023 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */

#include "com/centreon/broker/io/raw.hh"

namespace com::centreon::broker::io {

std::ostream& operator<<(std::ostream& s, const raw& d) {
  s << static_cast<const data&>(d) << " buffer_length:" << d.size();
  return s;
}

}  // namespace com::centreon::broker::io

using namespace com::centreon::broker::io;

/**
 *  Default constructor.
 */
raw::raw() : io::data(raw::static_type()) {}

/**
 *  Copy constructor.
 *
 *  @param[in] r Object to copy.
 */
raw::raw(raw const& r) : io::data(r), _buffer{r._buffer} {}

raw::raw(std::vector<char>&& b)
    : io::data(raw::static_type()), _buffer(std::move(b)) {}

raw::raw(const std::vector<char>& b)
    : io::data(raw::static_type()), _buffer(b) {}

raw::raw(char* dataptr, size_t r)
    : io::data(raw::static_type()), _buffer(dataptr, dataptr + r) {}
/**
 *  Destructor.
 */
raw::~raw() {}

/**
 *  Assignment operator.
 *
 *  @param[in] r Object to copy.
 *
 *  @return This object.
 */
raw& raw::operator=(raw const& r) {
  if (this != &r) {
    data::operator=(r);
    _buffer = r._buffer;
  }
  return *this;
}

void raw::resize(size_t s) {
  _buffer.resize(s);
}

char* raw::data() {
  return &_buffer[0];
}

char const* raw::const_data() const {
  return &_buffer[0];
}

size_t raw::size() const {
  return _buffer.size();
}

std::vector<char>& raw::get_buffer() {
  return _buffer;
}

bool raw::empty() const {
  return _buffer.empty();
}

// void raw::append(const char* msg) {
//  _buffer.insert(_buffer.end(), msg, msg + strlen(msg));
//}
