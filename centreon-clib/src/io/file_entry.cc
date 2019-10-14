/*
** Copyright 2012-2013 Centreon
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

#include <cerrno>
#include <cstdlib>
#include <cstring>
#ifndef _WIN32
#include <libgen.h>
#endif  // !_WIN32
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/io/file_entry.hh"

using namespace com::centreon::io;

/**
 *  Constructor.
 *
 *  @param[in] path  The file path.
 */
file_entry::file_entry(char const* path) : _path(path ? path : "") {
  refresh();
}

/**
 *  Constructor overload.
 */
file_entry::file_entry(std::string const& path) : _path(path) { refresh(); }

/**
 *  Copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
file_entry::file_entry(file_entry const& right) { _internal_copy(right); }

/**
 *  Destructor.
 */
file_entry::~file_entry() throw() {}

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
bool file_entry::operator==(file_entry const& right) const throw() {
  return (_sbuf.st_dev == right._sbuf.st_dev &&
          _sbuf.st_ino == right._sbuf.st_ino);
}

/**
 *  Not equal operator.
 *
 *  @param[in] right  The object to compare.
 *
 *  @return True if is not the same object, otherwise false.
 */
bool file_entry::operator!=(file_entry const& right) const throw() {
  return (!operator==(right));
}

/**
 *  Get the file name without extention.
 *
 *  @return The file name without extention.
 */
std::string file_entry::base_name() const {
  std::string name;
#ifdef _WIN32
  char fname[_MAX_FNAME];
  _splitpath(_path.c_str(), NULL, NULL, fname, NULL);
  name = fname;
#else
  name = file_name();
  size_t pos(name.find_last_of('.'));
  if (pos != 0 && pos != std::string::npos)
    name.erase(pos);
#endif  // _WIN32
  return (name);
}

/**
 *  Get the directory path.
 *
 *  @return The directory path of this file.
 */
std::string file_entry::directory_name() const {
  std::string name;
#ifdef _WIN32
  char drive[_MAX_DRIVE];
  char dir[_MAX_DIR];
  _splitpath(_path.c_str(), drive, dir, NULL, NULL);
  name = drive;
  name.append(dir);
#else
  char* path(new char[_path.size() + 1]);
  strcpy(path, _path.c_str());
  char* dname(dirname(path));
  name = dname;
  delete[] path;
#endif  // _WIN32
  return (name);
}

/**
 *  Get the file name without extention.
 *
 *  @return The file name without extention.
 */
std::string file_entry::file_name() const {
  std::string name;
#ifdef _WIN32
  char fname[_MAX_FNAME];
  char ext[_MAX_EXT];
  _splitpath(_path.c_str(), NULL, NULL, fname, ext);
  name = fname;
  name.append(ext);
#else
  char* path(new char[_path.size() + 1]);
  strcpy(path, _path.c_str());
  char* fname(basename(path));
  name = fname;
  delete[] path;
#endif  // _WIN32
  return (name);
}

/**
 *  Check if this file is a directory.
 *
 *  @return True if this file is a directory, otherwise false.
 */
bool file_entry::is_directory() const throw() {
  return ((_sbuf.st_mode & S_IFMT) == S_IFDIR);
}

/**
 *  Check if this file is a symbolic link.
 *
 *  @return True if this file is a symbolic link, otherwise false.
 */
bool file_entry::is_link() const throw() {
#ifdef _WIN32
  return (false);
#else
  return ((_sbuf.st_mode & S_IFMT) == S_IFLNK);
#endif  // Win32 or POSIX
}

/**
 *  Check if this file is regular.
 *
 *  @return True if this file is regular, otherwise false.
 */
bool file_entry::is_regular() const throw() {
  return ((_sbuf.st_mode & S_IFMT) == S_IFREG);
}

/**
 *  Get the file path.
 *
 *  @return The path.
 */
std::string const& file_entry::path() const throw() { return (_path); }

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
    throw(basic_error() << "get file information failed: " << msg);
  }
}

/**
 *  Get the file size.
 *
 *  @return The file size.
 */
unsigned long long file_entry::size() const throw() { return (_sbuf.st_size); }

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
