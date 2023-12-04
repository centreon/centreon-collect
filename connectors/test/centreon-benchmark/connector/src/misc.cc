/**
* Copyright 2011-2013 Centreon
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* For more information : contact@centreon.com
*/

#include "com/centreon/benchmark/connector/misc.hh"
#include <fstream>
#include "com/centreon/benchmark/connector/basic_exception.hh"

/**
 *  Load the commands configuration file.
 *
 *  @param[in] file  The file to parse.
 *
 *  @return All commands from the commands file.
 */
std::vector<std::string>
com::centreon::benchmark::connector::load_commands_file(
    std::string const& file) {
  std::vector<std::string> tab;
  std::ifstream is;
  is.open(file.c_str(), std::ios::in);
  if (!is.is_open())
    throw(basic_exception("open commands file failed"));
  while (is.good()) {
    std::string line;
    std::getline(is, line, '\n');
    if (!line.empty())
      tab.push_back(line);
  }
  is.close();
  return (tab);
}

/**
 *  Create string array from string list.
 *
 *  @param[in] v  The list to convert.
 *
 *  @return The new string array.
 */
char** com::centreon::benchmark::connector::list_to_tab(
    std::list<std::string> const& v,
    unsigned int size) {
  if (!size)
    size = v.size() + 1;
  char** tab(new char*[size]);
  unsigned int i(0);
  for (std::list<std::string>::const_iterator it(v.begin()), end(v.end());
       it != end; ++it)
    tab[i++] = const_cast<char*>(it->c_str());
  tab[i] = NULL;
  return (tab);
}
