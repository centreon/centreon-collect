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
#include <cstdlib>
#include <cstring>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/io/file_entry.hh"

using namespace com::centreon::io;

/**
 *  Constructor.
 *
 *  @param[in] path  The file path.
 */
file_entry::file_entry(char const* path)
  : _path(path ? path : "") {
  refresh();
}

/**
 *  Constructor overload.
 */
file_entry::file_entry(std::string const& path)
  : _path(path) {
  refresh();
}

/**
 *  Copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
file_entry::file_entry(file_entry const& right) {
  _internal_copy(right);
}

/**
 *  Destructor.
 */
file_entry::~file_entry() throw () {

}

/**
 *  Copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
file_entry& file_entry::operator=(file_entry const& right) {
  _internal_copy(right);
  return (*this);
}

/**
 *  Equal operator.
 *
 *  @param[in] right  The object to compare.
 *
 *  @return True if is the same object, otherwise false.
 */
bool file_entry::operator==(file_entry const& right) const throw () {
  return (_sbuf.st_dev == right._sbuf.st_dev
          && _sbuf.st_ino == right._sbuf.st_ino);
}

/**
 *  Not equal operator.
 *
 *  @param[in] right  The object to compare.
 *
 *  @return True if is not the same object, otherwise false.
 */
bool file_entry::operator!=(file_entry const& right) const throw () {
  return (!operator==(right));
}

/**
 *  Check if this file is a directory.
 *
 *  @return True if this file is a directory, otherwise false.
 */
bool file_entry::is_directory() const throw () {
  return ((_sbuf.st_mode & S_IFMT) == S_IFDIR);
}

/**
 *  Check if this file is a symbolic link.
 *
 *  @return True if this file is a symbolic link, otherwise false.
 */
bool file_entry::is_link() const throw () {
  return ((_sbuf.st_mode & S_IFMT) == S_IFLNK);
}

/**
 *  Check if this file is regular.
 *
 *  @return True if this file is regular, otherwise false.
 */
bool file_entry::is_regular() const throw () {
  return ((_sbuf.st_mode & S_IFMT) == S_IFREG);
}

/**
 *  Get the file path.
 *
 *  @return The path.
 */
std::string const& file_entry::path() const throw () {
  return (_path);
}

/**
 *  Set the file entry path.
 *
 *  @param[in] path  The file entry path.
 */
void file_entry::path(char const* path) {
  _path = path ? path : "";
  refresh();
}

/**
 *  Set the file entry path.
 *
 *  @param[in] path  The file entry path.
 */
void file_entry::path(std::string const& path) {
  _path = path;
  refresh();
}

/**
 *  Reload file information.
 */
void file_entry::refresh() {
  if (_path.empty())
    memset(&_sbuf, 0, sizeof(_sbuf));
  else if (stat(_path.c_str(), &_sbuf)) {
    char const* msg(strerror(errno));
    throw (basic_error() << "get file information failed: "
           << msg);
  }
}

/**
 *  Get the file size.
 *
 *  @return The file size.
 */
unsigned long long file_entry::size() const throw () {
  return (_sbuf.st_size);
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 */
void file_entry::_internal_copy(file_entry const& right) {
  if (this != &right) {
    _path = right._path;
    memcpy(&_sbuf, &right._sbuf, sizeof(_sbuf));
  }
}
