/*
** Copyright 2011-2013 Merethis
**
** This file is part of Centreon Connector ICMP.
**
** Centreon Connector ICMP is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector ICMP is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector ICMP. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <fstream>
#include "com/centreon/benchmark/connector/basic_exception.hh"
#include "com/centreon/benchmark/connector/misc.hh"

/**
 *  Load the commands configuration file.
 *
 *  @param[in] file  The file to parse.
 *
 *  @return All commands from the commands file.
 */
std::vector<std::string> com::centreon::benchmark::connector::load_commands_file(std::string const& file) {
  std::vector<std::string> tab;
  std::ifstream is;
  is.open(file.c_str(), std::ios::in);
  if (!is.is_open())
    throw (basic_exception("open commands file failed"));
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
  for (std::list<std::string>::const_iterator
         it(v.begin()), end(v.end());
       it != end;
       ++it)
    tab[i++] = const_cast<char*>(it->c_str());
  tab[i] = NULL;
  return (tab);
}
