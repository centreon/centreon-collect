/*
** Copyright 2011-2012 Merethis
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
