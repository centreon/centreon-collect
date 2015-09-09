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

#ifndef CC_TEST_LOGGING_BACKEND_TEST_HH
#  define CC_TEST_LOGGING_BACKEND_TEST_HH

#  include "com/centreon/concurrency/locker.hh"
#  include "com/centreon/logging/backend.hh"

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
