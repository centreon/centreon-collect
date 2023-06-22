/*
** Copyright 2023 Centreon
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

#ifndef CCCM_NAMESPACE_HH
#define CCCM_NAMESPACE_HH

#ifdef CCCM_BEGIN
#undef CCCM_BEGIN
#endif  // CCCM_BEGIN
#define CCCM_BEGIN()   \
  namespace com {      \
  namespace centreon { \
  namespace common {

#ifdef CCCM_END
#undef CCCM_END
#endif  // CCCM_END
#define CCCM_END() \
  }                \
  }                \
  }

#endif  // !CCCM_NAMESPACE_HH
