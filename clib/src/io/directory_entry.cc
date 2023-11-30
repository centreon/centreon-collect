/**
* Copyright 2012-2013 Centreon
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

#include "com/centreon/io/directory_entry.hh"
#include <dirent.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include "com/centreon/exceptions/basic.hh"

using namespace com::centreon::io;

/**
 *  Constructor.
 *
 *  @param[in] path  The directory path.
 */
directory_entry::directory_entry(char const* path) : _entry(path) {}

/**
 *  Constructor.
 *
 *  @param[in] path  The directory path.
 */
directory_entry::directory_entry(std::string const& path) : _entry(path) {}

/**
 *  Copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
directory_entry::directory_entry(directory_entry const& right) {
  _internal_copy(right);
}

/**
 *  Copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
directory_entry& directory_entry::operator=(directory_entry const& right) {
  _internal_copy(right);
  return *this;
}

/**
 *  Equal operator.
 *
 *  @param[in] right  The object to compare.
 *
 *  @return True if is the same object, owtherwise false.
 */
bool directory_entry::operator==(directory_entry const& right) const noexcept {
  return _entry == right._entry;
}

/**
 *  Not equal operator.
 *
 *  @param[in] right  The object to compare.
 *
 *  @return True if is not the same object, owtherwise false.
 */
bool directory_entry::operator!=(directory_entry const& right) const noexcept {
  return !operator==(right);
}

/**
 *  Destructor.
 */
directory_entry::~directory_entry() noexcept {}

/**
 *  Get the current directory path.
 *
 *  @return The current directory path.
 */
std::string directory_entry::current_path() {
  char* buffer(getcwd(NULL, 0));
  if (!buffer)
    throw(basic_error() << "current path failed");
  std::string path(buffer);
  free(buffer);
  return path;
}

/**
 *  Get the directory information.
 *
 *  @return The directory entry.
 */
file_entry const& directory_entry::entry() const noexcept {
  return _entry;
}

/**
 *  Get the list of all entry into a directory.
 *
 *  @param[in] filter  An optional filter.
 *
 *  @return The file entry list.
 */
std::list<file_entry> const& directory_entry::entry_list(
    std::string const& filter) {
  _entry_lst.clear();
  char const* filter_ptr(filter.empty() ? NULL : filter.c_str());

  DIR* dir(opendir(_entry.path().c_str()));
  if (!dir) {
    char const* msg(strerror(errno));
    throw(basic_error() << "open directory failed: " << msg);
  }

  dirent entry;
  dirent* result;
  while (true) {
    if (readdir_r(dir, &entry, &result)) {
      closedir(dir);
      throw(basic_error() << "parse directory failed");
    }
    if (!result)
      break;
    if (!filter_ptr || _nmatch(entry.d_name, filter_ptr))
      _entry_lst.push_back(file_entry(_entry.path() + "/" + entry.d_name));
  }
  closedir(dir);

  return _entry_lst;
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 */
void directory_entry::_internal_copy(directory_entry const& right) {
  if (this != &right) {
    _entry = right._entry;
    _entry_lst = right._entry_lst;
  }
}

/**
 *  Check if a string match a pattern.
 *
 *  @param[in] str      The string to check.
 *  @param[in] pattern  The partter to match.
 *
 *  @return 1 on success, otherwiswe 0.
 */
int directory_entry::_nmatch(char const* str, char const* pattern) {
  if (!*str && !*pattern)
    return 1;
  if (*str == *pattern)
    return _nmatch(str + 1, pattern + 1);
  return (*pattern == '*' ? (*str ? _nmatch(str + 1, pattern) : 0) +
                                _nmatch(str, pattern + 1)
                          : 0);
}
