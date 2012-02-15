/*
** Copyright 2012 Merethis
**
** This file is part of Centreon Clib.
**
** Centreon Clib is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Clib is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Clib. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CC_IO_FILE_STREAM_HH
#  define CC_IO_FILE_STREAM_HH

#  include <stdio.h>
#  include "com/centreon/handle.hh"
#  include "com/centreon/namespace.hh"

CC_BEGIN()

namespace         io {
  /**
   *  @class file_stream file_stream.hh "com/centreon/io/file_stream.hh"
   *  @brief Wrapper of libc's FILE streams.
   *
   *  Wrap standard FILE stream objects.
   */
  class           file_stream : public handle {
  public:
                  file_stream(
                    FILE* stream = NULL,
                    bool auto_close = false);
                  ~file_stream() throw ();
    void          close();
    native_handle get_native_handle();
    void          open(char const* path, char const* mode);
    unsigned long read(void* data, unsigned long size);
    static char*  temp_path();
    unsigned long write(void const* data, unsigned long size);

  private:
                  file_stream(file_stream const& fs);
    file_stream&  operator=(file_stream const& fs);
    void          _internal_copy(file_stream const& fs);

    bool          _auto_close;
    FILE*         _stream;
  };
}

CC_END()

#endif // !CC_IO_FILE_STREAM_HH
