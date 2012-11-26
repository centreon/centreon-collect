/*
** Copyright 2011 Merethis
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

#ifndef CC_TEST_LOGGING_BACKEND_TEST_HH
# define CC_TEST_LOGGING_BACKEND_TEST_HH

#include "com/centreon/concurrency/locker.hh"
#include "com/centreon/logging/backend.hh"

CC_BEGIN()

namespace              logging {
  /**
   *  @class backend_test
   *  @brief litle implementation of backend to test logging engine.
   */
  class                backend_test : public backend {
  public:
                       backend_test(
                         bool is_sync = false,
                         bool show_pid = false,
                         time_precision show_timestamp = none,
                         bool show_thread_id = false)
                         : backend(
                             is_sync,
                             show_pid,
                             show_timestamp,
                             show_thread_id),
                           _nb_call(0) {}
                       ~backend_test() throw () {}
    void               close() throw () {}
    std::string const& data() const throw () { return (_buffer); }
    void               log(
                         unsigned long long types,
                         unsigned int verbose,
                         char const* msg,
                         unsigned int size) throw () {
      concurrency::locker lock(&_lock);

      (void)types;
      (void)verbose;

      misc::stringifier header;
      _build_header(header);
      _buffer.append(header.data(), header.size());
      _buffer.append(msg, size);
      ++_nb_call;
    }
    unsigned int       get_nb_call() const throw () {
      return (_nb_call);
    }
    void               open() {}
    void               reopen() {}
    void               reset() throw () { _buffer.clear(); }

  private:
    std::string        _buffer;
    unsigned int       _nb_call;
  };
}

CC_END()

#endif // !CC_TEST_LOGGING_BACKEND_TEST_HH
