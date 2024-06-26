/**
 * Copyright 2011 - 2019 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */

#include "com/centreon/broker/stats/worker_pool.hh"

#include <sys/stat.h>

#include <cerrno>

#include "com/centreon/exceptions/msg_fmt.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::exceptions;
using namespace com::centreon::broker;
using namespace com::centreon::broker::stats;
using log_v2 = com::centreon::common::log_v2::log_v2;

worker_pool::worker_pool() {}

void worker_pool::add_worker(std::string const& fifo) {
  // Does file exist and is a FIFO ?
  struct stat s;
  std::string fifo_path = fifo;
  // Stat failed, probably because of inexistant file.
  if (stat(fifo_path.c_str(), &s) != 0) {
    char const* msg(strerror(errno));
    log_v2::instance()
        .get(log_v2::STATS)
        ->info("stats: cannot stat() '{}': {}", fifo_path, msg);

    // Create FIFO.
    if (mkfifo(fifo_path.c_str(),
               S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH) != 0) {
      char const* msg(strerror(errno));
      throw msg_fmt("cannot create FIFO '{}': {}", fifo_path, msg);
    }
  } else if (!S_ISFIFO(s.st_mode))
    throw msg_fmt("file '{}' exists but is not a FIFO", fifo_path);

  // Create thread.
  _workers_fifo.push_back(std::make_shared<stats::worker>());
  _workers_fifo.back()->run(fifo_path);
}

void worker_pool::cleanup() {
  _workers_fifo.clear();
}
