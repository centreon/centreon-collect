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

#include <iostream>
#include <memory>
#include <sstream>
#include <thread>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/engine.hh"
#include "./backend_test.hh"

using namespace com::centreon::logging;

/**
 *  Check add backend on to the logging engine.
 *
 *  @return 0 on success.
 */
int main() {
  static uint32_t const nb_writter(10);
  static uint32_t const nb_write(10);
  int retval;

  engine::load();
  try {
    engine& e(engine::instance());

    std::unique_ptr<backend_test> obj(new backend_test);
    e.add(obj.get(), 1, 0);

    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < nb_writter; ++i)
      threads.push_back(std::thread([]() {
        engine& e(engine::instance());
        for (uint32_t i = 0; i < nb_writter; ++i) {
          std::ostringstream oss;
          oss << std::this_thread::get_id() << ":" << i;
          e.log(1, 0, oss.str().c_str(), oss.str().size());
        }
      }));

    for (auto& t : threads)
      t.join();

    for (uint32_t i(0); i < nb_writter; ++i) {
      for (uint32_t j(0); j < nb_writter; ++j) {
        std::ostringstream oss;
        oss << &threads[i] << ":" << j << "\n";
        if (!obj->data().find(oss.str()))
          throw basic_error() << "pattern not found";
      }
    }
    retval = 0;
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    retval = 1;
  }
  engine::unload();
  return retval;
}
