/*
** Copyright 2011 Merethis
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

#include <iostream>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/handle_manager.hh"

using namespace com::centreon;

/**
 *  @class standard
 *  @brief litle implementation of handle to test the handle manager.
 */
class           file_descriptor : public handle {
public:
                file_descriptor(int fd)
                  : handle(fd) {}
                ~file_descriptor() throw () {}
  void          close() {}
  unsigned long read(void* data, unsigned long size) {
    int ret(::read(_internal_handle, data, size));
    if (ret < 0)
      throw (basic_error() << "read failed:" << strerror(errno));
    return (static_cast<unsigned long>(ret));
  }
  unsigned long write(void const* data, unsigned long size) {
    int ret(::write(_internal_handle, data, size));
    if (ret < 0)
      throw (basic_error() << "write failed:" << strerror(errno));
    return (static_cast<unsigned long>(ret));
  }
};

/**
 *  @class listener
 *  @brief litle implementation of handle listener to test the
 *         handle manager.
 */
class     listener : public handle_listener {
public:
          listener(
            handle& ref_h,
            bool want_read,
            bool want_write)
            : _is_call(false),
              _ref_h(ref_h),
              _want_read(want_read),
              _want_write(want_write) {}
          ~listener() throw () {}
  void    close(handle& h) { if (&_ref_h == &h) _is_call = true; }
  void    error(handle& h) { if (&_ref_h == &h) _is_call = true; }
  bool    is_call() const throw () { return (_is_call); }
  void    read(handle& h) { if (&_ref_h == &h) _is_call = true; }
  bool    want_read(handle& h) { (void)h; return (_want_read); }
  bool    want_write(handle& h) { (void)h; return (_want_write); }
  void    write(handle& h) { if (&_ref_h == &h) _is_call = true; }

private:
  bool    _is_call;
  handle& _ref_h;
  bool    _want_read;
  bool    _want_write;
};

/**
 *  Try to insert a null task manager into handle manager.
 *
 *  @return True on success, otherwise false.
 */
static bool null_task_manager() {
  try {
    handle_manager hm;
    hm.multiplex();
  }
  catch (std::exception const& e) {
    (void)e;
    return (true);
  }
  return (false);
}

/**
 *  Try to execute without any handle.
 *
 *  @return True on success, otherwise false.
 */
static bool empty_handle_manager() {
  try {
    task_manager tm;
    handle_manager hm(&tm);
    hm.multiplex();
  }
  catch (std::exception const& e) {
    (void)e;
    return (false);
  }
  return (true);
}

/**
 *  Check if write is calling.
 *
 *  @return True on success, otherwise false.
 */
static bool basic_multiplex_write() {
  task_manager tm;
  handle_manager hm(&tm);
  file_descriptor fd(1);
  listener l(fd, false, true);
  hm.add(&fd, &l);
  hm.multiplex();
  return (l.is_call());
}

/**
 *  Check if error is calling.
 *
 *  @return True on success, otherwise false.
 */
static bool basic_multiplex_error() {
  task_manager tm;
  handle_manager hm(&tm);
  file_descriptor fd(42);
  listener l(fd, true, true);
  hm.add(&fd, &l);
  hm.multiplex();
  return (l.is_call());
}

/**
 *  Check if close is calling.
 *
 *  @return True on success, otherwise false.
 */
static bool basic_multiplex_close() {
  ::close(0);
  task_manager tm;
  handle_manager hm(&tm);
  file_descriptor fd(0);
  listener l(fd, true, true);
  hm.add(&fd, &l);
  hm.multiplex();
  return (l.is_call());
}

/**
 *  Check the handle manager multiplexing.
 *
 *  @return 0 on success.
 */
int main() {
  try {
    if (!null_task_manager())
      throw (basic_error() << "");
    if (!empty_handle_manager())
      throw (basic_error() << "");
    if (!basic_multiplex_write())
      throw (basic_error() << "multiplex one handle to write failed");
    if (!basic_multiplex_error())
      throw (basic_error() << "multiplex one handle to error failed");
    if (!basic_multiplex_close())
      throw (basic_error() << "multiplex one handle to close failed");
  }
  catch (std::exception const& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return (1);
  }
  return (0);
}
