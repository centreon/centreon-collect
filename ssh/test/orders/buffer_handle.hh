/*
** Copyright 2011-2013 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef TEST_ORDERS_BUFFER_HANDLE_HH
#  define TEST_ORDERS_BUFFER_HANDLE_HH

#  include <string>
#  include "com/centreon/handle.hh"

/**
 *  @class buffer_handle buffer_handle.hh "test/orders/buffer_handle.hh"
 *  @brief Buffer that can serve as handle.
 *
 *  Bufferize data and make it available through read.
 */
class            buffer_handle : public com::centreon::handle {
public:
                 buffer_handle();
                 buffer_handle(buffer_handle const& bh);
                 ~buffer_handle() throw ();
  buffer_handle& operator=(buffer_handle const& bh);
  void           close();
  bool           empty() const;
  com::centreon::native_handle
                 get_native_handle();
  unsigned long  read(void* data, unsigned long size);
  unsigned long  write(void const* data, unsigned long size);

private:
  void           _copy(buffer_handle const& bh);

  std::string    _buffer;
};

#endif // !TEST_ORDERS_BUFFER_HANDLE
