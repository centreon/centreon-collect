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
 *  Check that credentials can be copy constructed.
 *
 *  @return 0 on success.
 */
int main() {
  // Base object.
  credentials creds1;
  creds1.set_host("localhost");
  creds1.set_user("root");
  creds1.set_password("random words");

  // Copy object.
  credentials creds2(creds1);

  // Reset base object.
  creds1.set_host("centreon.com");
  creds1.set_user("daemon");
  creds1.set_password("please let me in");

  // Check.
  return ((creds1.get_host() != "centreon.com") ||
          (creds1.get_user() != "daemon") ||
          (creds1.get_password() != "please let me in") ||
          (creds2.get_host() != "localhost") || (creds2.get_user() != "root") ||
          (creds2.get_password() != "random words"));
}
