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

#ifndef CC_TEST_CONCURRENCY_THREAD_TEST_HH
#define CC_TEST_CONCURRENCY_THREAD_TEST_HH

#include <thread>

CC_BEGIN()

namespace concurrency {
/**
 *  @class thread_test
 *  @brief litle implementation of thread to test concurrency thread.
 */
class thread_test {
 public:
  thread_test() : _quit(false) {
    _thread = new std::thread(&thread_test::_run, this);
  }

  ~thread_test() {
    _thread->join();
    delete _thread;
  }

  void quit() { _quit = true; }

 private:
  std::thread* _thread;

  void _run() {
    while (!_quit)
      std::this_thread::yield();
  }
  bool _quit;
};
}

CC_END()

#endif  // !CC_TEST_CONCURRENCY_THREAD_TEST_HH
