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

#ifndef CCCS_SESSIONS_SOCKET_HANDLE_HH
#define CCCS_SESSIONS_SOCKET_HANDLE_HH

#include "com/centreon/connector/ssh/namespace.hh"
#include "com/centreon/handle.hh"

CCCS_BEGIN()

namespace sessions {
/**
 *  @class socket_handle socket_handle.hh
 * "com/centreon/connector/ssh/socket_handle.hh"
 *  @brief Socket handle.
 *
 *  Wrapper around a socket descriptor.
 */
class socket_handle : public com::centreon::handle {
 public:
  socket_handle(native_handle handl = native_handle_null);
  ~socket_handle() noexcept;
  void close();
  native_handle get_native_handle();
  unsigned long read(void* data, unsigned long size);
  void set_native_handle(native_handle handl);
  unsigned long write(void const* data, unsigned long size);

 private:
  socket_handle(socket_handle const& sh);
  socket_handle& operator=(socket_handle const& sh);

  native_handle _handl;
};
}  // namespace sessions

CCCS_END()

#endif  // !CCCS_SESSIONS_SOCKET_HANDLE_HH
