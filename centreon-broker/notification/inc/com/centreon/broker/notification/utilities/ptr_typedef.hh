/*
** Copyright 2011-2014 Centreon
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

#ifndef CCB_NOTIFICATION_UTILITIES_PTR_TYPEDEF_HH
#define CCB_NOTIFICATION_UTILITIES_PTR_TYPEDEF_HH

#include <memory>

#define DECLARE_SHARED_PTR(klass) typedef std::shared_ptr<klass> ptr

#endif  //! CCB_NOTIFICATION_UTILITIES_PTR_TYPEDEF_HH
