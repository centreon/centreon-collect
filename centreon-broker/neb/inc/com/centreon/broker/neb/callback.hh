/*
** Copyright 2012 Centreon
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

#ifndef CCB_NEB_CALLBACK_HH
#define CCB_NEB_CALLBACK_HH

#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace neb {
/**
 *  @class callback callback.hh "com/centreon/broker/neb/callback.hh"
 *  @brief Manager NEB callbacks.
 *
 *  Handle callback registration/deregistration with Nagios.
 */
class callback {
 public:
  callback(int id, void* handle, int (*function)(int, void*));
  ~callback() throw();

 private:
  callback(callback const& right);
  callback& operator=(callback const& right);

  int (*_function)(int, void*);
  int _id;
};
}  // namespace neb

CCB_END()

#endif /* !CCB_NEB_CALLBACK_HH */
