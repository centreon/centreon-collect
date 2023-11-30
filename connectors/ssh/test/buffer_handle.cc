/**
* Copyright 2011-2013 Centreon
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

#include <cstring>
#include "buffer_handle.hh"

using namespace com::centreon;

/**
 *  Close handle.
 */
void buffer_handle::close() {
  _buffer.clear();
}

/**
 *  Check if buffer is empty.
 *
 *  @return true if buffer is empty.
 */
bool buffer_handle::empty() const { return _buffer.empty(); }

/**
 *  Get the native handle.
 *
 *  @return Invalid handle.
 */
native_handle buffer_handle::get_native_handle() {
  return native_handle_null;
}

/**
 *  Read data.
 *
 *  @param[in] data Destination buffer.
 *  @param[in] size Maximum size to read.
 *
 *  @return Number of bytes read.
 */
unsigned long buffer_handle::read(void* data, unsigned long size) {
  // Check size to read.
  {
    unsigned long buffer_size(_buffer.size());
    if (size > buffer_size)
      size = buffer_size;
  }

  // Read data.
  memcpy(data, _buffer.c_str(), size);
  _buffer.erase(0, size);
  return size;
}

/**
 *  Write data.
 *
 *  @param[in] data Source buffer.
 *  @param[in] size Size to write.
 *
 *  @return size.
 */
unsigned long buffer_handle::write(void const* data, unsigned long size) {
  _buffer.append(static_cast<char const*>(data), size);
  return size;
}
