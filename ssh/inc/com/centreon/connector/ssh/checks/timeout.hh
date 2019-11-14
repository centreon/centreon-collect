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

#ifndef CCCS_CHECKS_TIMEOUT_HH
#  define CCCS_CHECKS_TIMEOUT_HH

#  include <cstddef>
#  include "com/centreon/connector/ssh/namespace.hh"
#  include "com/centreon/task.hh"

CCCS_BEGIN()

namespace    checks {
  // Forward declaration.
  class      check;

  /**
   *  @class timeout timeout.hh "com/centreon/connector/ssh/checks/timeout.hh"
   *  @brief Check timeout.
   *
   *  Task executed when a check timeouts.
   */
  class      timeout : public com::centreon::task {
  public:
             timeout(check* chk = NULL);
             timeout(timeout const& t) = delete;
             ~timeout() throw ();
    timeout& operator=(timeout const& t) = delete;
    check*   get_check() const throw ();
    void     run();
    void     set_check(check* chk) throw ();

  private:
    void     _internal_copy(timeout const& t);

    check*   _check;
  };
}

CCCS_END()

#endif // !CCCS_CHECKS_TIMEOUT_HH
