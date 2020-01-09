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

#include "com/centreon/connector/ssh/sessions/credentials.hh"

using namespace com::centreon::connector::ssh::sessions;

/**
 *  Check credentials' less-than operator.
 *
 *  @return 0 on success.
 */
int main() {
  // Objects.
  credentials creds1("AAA", "GGG", "VVV");
  credentials creds2("BBB", "HHH", "XXX");
  credentials creds3("AAA", "HHH", "QQQ");
  credentials creds4("AAA", "GGG", "ZZZ");
  credentials creds5(creds1);

  // Checks.
  return (!(creds1 < creds2) || !(creds1 < creds3) || !(creds1 < creds4) ||
          (creds1 < creds5) || (creds5 < creds1));
}
