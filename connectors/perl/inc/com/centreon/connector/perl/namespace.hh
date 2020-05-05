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

#ifndef CCCP_NAMESPACE_HH
#define CCCP_NAMESPACE_HH

#ifdef CCCP_BEGIN
#undef CCCP_BEGIN
#endif  // CCCP_BEGIN
#define CCCP_BEGIN()    \
  namespace com {       \
  namespace centreon {  \
  namespace connector { \
  namespace perl {

#ifdef CCCP_END
#undef CCCP_END
#endif  // CCCP_END
#define CCCP_END() \
  }                \
  }                \
  }                \
  }

#endif  // !CCCP_NAMESPACE_HH
