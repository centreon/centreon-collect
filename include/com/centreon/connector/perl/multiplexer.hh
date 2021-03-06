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

#ifndef CCCP_MULTIPLEXER_HH
#define CCCP_MULTIPLEXER_HH

#include "com/centreon/connector/perl/namespace.hh"
#include "com/centreon/handle_manager.hh"
#include "com/centreon/task_manager.hh"

CCCP_BEGIN()

/**
 *  @class multiplexer multiplexer.hh
 * "com/centreon/connector/perl/multiplexer.hh"
 *  @brief Multiplexing class.
 *
 *  Singleton that aggregates multiplexing features such as file
 *  descriptor monitoring and task execution.
 */
class multiplexer : public com::centreon::task_manager,
                    public com::centreon::handle_manager {
 public:
  static void load();
  static void unload();
  multiplexer(multiplexer const& m) = delete;
  static multiplexer& instance() noexcept;

 private:
  multiplexer();
};

CCCP_END()

#endif  // !CCCP_MULTIPLEXER_HH
