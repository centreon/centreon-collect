/*
** Copyright 2012-2014 Centreon
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

#ifndef CCCP_PIPE_HANDLE_HH
#define CCCP_PIPE_HANDLE_HH

#include "com/centreon/connector/perl/namespace.hh"
#include "com/centreon/handle.hh"

CCCP_BEGIN()

/**
 *  @class pipe_handle pipe_handle.hh
 * "com/centreon/connector/perl/pipe_handle.hh"
 *  @brief Wrap a pipe FD.
 *
 *  Wrap a pipe file descriptor within a class.
 */
class pipe_handle : public handle {
  int _fd;

 public:
  pipe_handle(int fd = -1);
  ~pipe_handle() noexcept;
  pipe_handle(pipe_handle const& ph) = delete;
  pipe_handle& operator=(pipe_handle const& ph) = delete;
  void close() noexcept;
  static void close_all_handles();
  native_handle get_native_handle() noexcept;
  unsigned long read(void* data, unsigned long size);
  void set_fd(int fd);
  unsigned long write(void const* data, unsigned long size);
};

CCCP_END()

#endif  // !CCCP_PIPE_HANDLE_HH
