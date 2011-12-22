/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Connector SSH.
**
** Centreon Connector SSH is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector SSH is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector SSH. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <string.h>
#include "test/orders/buffer_handle.hh"

using namespace com::centreon;

/**
 *  Default constructor.
 */
buffer_handle::buffer_handle() {}

/**
 *  Copy constructor.
 *
 *  @param[in] bh Object to copy.
 */
buffer_handle::buffer_handle(buffer_handle const& bh) : handle(bh) {
  _copy(bh);
}

/**
 *  Destructor.
 */
buffer_handle::~buffer_handle() throw () {}

/**
 *  Assignment operator.
 *
 *  @param[in] bh Object to copy.
 *
 *  @return This object.
 */
buffer_handle& buffer_handle::operator=(buffer_handle const& bh) {
  if (this != &bh) {
    handle::operator=(bh);
    _copy(bh);
  }
  return (*this);
}

/**
 *  Close handle.
 */
void buffer_handle::close() {
  _buffer.clear();
  return ;
}

/**
 *  Check if buffer is empty.
 *
 *  @return true if buffer is empty.
 */
bool buffer_handle::empty() const {
  return (_buffer.empty());
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
  return (size);
}

/**
 *  Write data.
 *
 *  @param[in] data Source buffer.
 *  @param[in] size Size to write.
 *
 *  @return size.
 */
unsigned long buffer_handle::write(
                               void const* data,
                               unsigned long size) {
  _buffer.append(static_cast<char const*>(data), size);
  return (size);
}

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Copy internal data members.
 *
 *  @param[in] bh Object to copy.
 */
void buffer_handle::_copy(buffer_handle const& bh) {
  _buffer = bh._buffer;
  return ;
}
