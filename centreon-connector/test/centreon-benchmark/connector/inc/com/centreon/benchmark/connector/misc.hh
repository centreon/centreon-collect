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

#ifndef CCB_CONNECTOR_MISC
#define CCB_CONNECTOR_MISC

#include <list>
#include <string>
#include <vector>
#include "com/centreon/benchmark/connector/namespace.hh"

CCB_CONNECTOR_BEGIN()

std::vector<std::string> load_commands_file(std::string const& file);
char** list_to_tab(std::list<std::string> const& v, unsigned int size = 0);

CCB_CONNECTOR_END()

#endif  // !CCB_CONNECTOR_MISC
