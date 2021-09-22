/*
** Copyright 2011-2020 Centreon
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

#ifndef CCC_NAMESPACE_HH
#define CCC_NAMESPACE_HH

#ifdef CCC_BEGIN
#undef CCC_BEGIN
#endif  // CCC_BEGIN
#define CCC_BEGIN()    \
  namespace com {       \
  namespace centreon {  \
  namespace connector { \

#ifdef CCC_END
#undef CCC_END
#endif  // CCC_END
#define CCC_END() \
  }                \
  }                \
  }

#endif  // !CCC_NAMESPACE_HH
