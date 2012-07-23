/*
** Copyright 2012 Merethis
**
** This file is part of Centreon Clib.
**
** Centreon Clib is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Clib is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Clib. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <cerrno>
#include <cstring>
#ifdef _WIN32
#  include <windows.h>
#else
#  include <dirent.h>
#endif // _WIN32
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/io/directory_entry.hh"

using namespace com::centreon::io;

/**
 *  Constructor.
 *
 *  @param[in] path  The directory path.
 */
directory_entry::directory_entry(char const* path)
  : _entry(path) {

}

/**
 *  Constructor.
 *
 *  @param[in] path  The directory path.
 */
directory_entry::directory_entry(std::string const& path)
  : _entry(path) {

}

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
  return (*this);
}

/**
 *  Equal operator.
 *
 *  @param[in] right  The object to compare.
 *
 *  @return True if is the same object, owtherwise false.
 */
bool directory_entry::operator==(directory_entry const& right) const throw () {
  return (_entry == right._entry);
}

/**
 *  Not equal operator.
 *
 *  @param[in] right  The object to compare.
 *
 *  @return True if is not the same object, owtherwise false.
 */
bool directory_entry::operator!=(directory_entry const& right) const throw () {
  return (!operator==(right));
}

/**
 *  Destructor.
 */
directory_entry::~directory_entry() throw () {

}

/**
 *  Get the directory information.
 *
 *  @return The directory entry.
 */
file_entry const& directory_entry::entry() const throw () {
  return (_entry);
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

#ifdef _WIN32
  WIN32_FIND_DATA ffd;
  HANDLE hwd(FindFirstFile(_entry.path(), &ffd));
  if (hwd == INVALID_HANDLE_VALUE)
    throw (basic_error() << "open directory failed");

  do {
    if (!filter_ptr || _nmatch(ffd.cFileName, filter_ptr))
      _entry_lst.push_back(file_entry(ffd.cFileName));
  } while (FindNextFile(hwd, &ffd) != 0);
  DWORD error(GetLastError());
  FindClose(hwd);
  if (error != ERROR_NO_MORE_FILES)
    throw (basic_error() << "parse directory failed");
#else
  DIR* dir(opendir(_entry.path().c_str()));
  if (!dir) {
    char const* msg(strerror(errno));
    throw (basic_error() << "open directory failed: " << msg);
  }

  dirent entry;
  dirent* result;
  while (true) {
    if (readdir_r(dir, &entry, &result)) {
      closedir(dir);
      throw (basic_error() << "parse directory failed");
    }
    if (!result)
      break;
    if (!filter_ptr || _nmatch(entry.d_name, filter_ptr))
      _entry_lst.push_back(file_entry(entry.d_name));
  }
  closedir(dir);
#endif // _WIN32

  return (_entry_lst);
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
    return (1);
  if (*str == *pattern)
    return (_nmatch(str + 1, pattern + 1));
  return (*pattern == '*'
          ? (*str
             ? _nmatch(str + 1, pattern)
             : 0) + _nmatch(str, pattern + 1)
          : 0);
}
