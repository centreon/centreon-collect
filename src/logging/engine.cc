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

#ifdef _WIN32
#  include <windows.h>
#else
#  include <sys/types.h>
#  include <unistd.h>
#endif // _WIN32

#include <assert.h>
#include <memory>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "com/centreon/concurrency/thread.hh"
#include "com/centreon/exception/basic.hh"
#include "com/centreon/misc/stringifier.hh"
#include "com/centreon/timestamp.hh"
#include "com/centreon/logging/engine.hh"

using namespace com::centreon::concurrency;
using namespace com::centreon::logging;
using namespace com::centreon::misc;

engine* engine::_instance = NULL;

/**
 *  Add backend into the logging engine.
 *
 *  @param[in] obj      The backend to add into the logging engine.
 *  @param[in] types    The types to log with this backend.
 *  @param[in] verbose  The verbosity level to log with this backend.
 *
 *  @return The id of backend into the logging engine.
 */
unsigned long engine::add(
                        backend* obj,
                        type_flags types,
                        verbosity const& verbose) {
  if (!obj)
    throw (basic_error() << "add backend on the logging engine " \
           "failed:bad argument (null pointer)");

  std::auto_ptr<backend_info> info(new backend_info);
  info->id = ++_id;
  info->obj = obj;
  info->types = types;
  info->verbose = verbose;

  for (unsigned int i(0); i < sizeof(type_flags) * CHAR_BIT; ++i)
    if ((types & (static_cast<type_flags>(1) << i))
        && _list_verbose[i] < verbose)
      _list_verbose[i] = verbose;

  _backends.push_back(info.get());
  return (info.release()->id);
}


/**
 *  Get the logger engine singleton.
 *
 *  @return The unique instance of logger engine.
 */
engine& engine::instance() {
  return (*_instance);
}

/**
 *  Check if at least one backend can log with this parameter.
 *
 *  @param[in] flag     The logging type to log.
 *  @param[in] verbose  The verbosity level.
 *
 *  @return True if at least one backend can log with this parameter,
 *          otherwise false.
 */
bool engine::is_log(
               type_number flag,
               verbosity const& verbose) const throw () {
  if (flag >= sizeof(type_flags) * CHAR_BIT)
    return (false);
  return (_list_verbose[flag] != verbosity()
          && _list_verbose[flag] <= verbose);
}

/**
 *  Get if the pid is display.
 *
 *  @return True if pid is display, otherwise false.
 */
bool engine::get_enable_pid() const throw () {
  return (_show_pid);
}

/**
 *  Get if the timestamp is display.
 *
 *  @return Time precision is display, otherwise none.
 */
engine::time_precision engine::get_enable_timestamp() const throw () {
  return (_show_timestamp);
}

/**
 *  Get if the thread id is display.
 *
 *  @return True if thread id is display, otherwise false.
 */
bool engine::get_enable_thread_id() const throw () {
  return (_show_thread_id);
}

/**
 *  Create a new instance of the logging engine if no instance exist.
 */
void engine::load() {
  if (!_instance)
    _instance = new engine();
}

/**
 *  Log messages.
 *
 *  @param[in] flag     The logging type to log.
 *  @param[in] verbose  The verbosity level.
 *  @param[in] msg      The string to log.
 */
void engine::log(
               type_number flag,
               verbosity const& verbose,
               char const* msg) {
  if (!msg || !is_log(flag, verbose))
    return;

  // Build line header.
  stringifier header;
  if (_show_timestamp == second)
    header << "[" << timestamp::now().to_second() << "] ";
  else if (_show_timestamp == millisecond)
    header << "[" << timestamp::now().to_msecond() << "] ";
  else if (_show_timestamp == microsecond)
    header << "[" << timestamp::now().to_usecond() << "] ";
  if (_show_pid) {
#ifdef _WIN32
    header << "[" << GetCurrentProcessId() << "] ";
#else
    header << "[" << getpid() << "] ";
#endif
  }
  if (_show_thread_id)
    header << "[" << thread::get_current_id() << "] ";

  // Split msg by line.
  stringifier buffer;
  unsigned int i(0);
  unsigned int last(0);
  while (msg[i]) {
    if (msg[i] == '\n') {
      buffer << header;
      buffer.append(msg + last, i - last) << "\n";
      last = i + 1;
    }
    ++i;
  }
  if (last != i) {
    buffer << header;
    buffer.append(msg + last, i - last) << "\n";
  }

  for (std::vector<backend_info*>::const_iterator
         it(_backends.begin()), end(_backends.end());
       it != end;
       ++it)
    if (((*it)->types & (static_cast<type_flags>(1) << flag))
        && (*it)->verbose != verbosity()
        && (*it)->verbose <= verbose)
      (*it)->obj->log(buffer.data(), buffer.size());
}

/**
 *  Remove backend by id.
 *
 *  @param[in] id  The backend id.
 *
 *  @return True if the backend was remove, otherwise false.
 */
bool engine::remove(unsigned long id) {
  for (std::vector<backend_info*>::iterator
         it(_backends.begin()), end(_backends.end());
       it != end;
       ++it)
    if ((*it)->id == id) {
      delete *it;
      _backends.erase(it);
      _rebuild_verbosities();
      return (true);
    }
  return (false);
}

/**
 *  Remove backend.
 *
 *  @param[in] obj  The specific backend.
 *
 *  @return The number of backend was remove.
 */
unsigned int engine::remove(backend* obj) {
  if (!obj)
    throw (basic_error() << "remove backend on the logging engine " \
           "failed:bad argument (null pointer)");

  std::vector<backend_info*>::iterator it(_backends.begin());
  unsigned int count_remove(0);
  while (it != _backends.end()) {
    if ((*it)->obj != obj)
      ++it;
    else {
      delete *it;
      it = _backends.erase(it);
      ++count_remove;
    }
  }
  if (count_remove)
    _rebuild_verbosities();
  return (count_remove);
}

/**
 *  Set pid display.
 *
 *  @param[in] enable  Enable or disable display pid.
 */
void engine::set_enable_pid(bool enable) throw () {
  _show_pid = enable;
}

/**
 *  Set timestamp display.
 *
 *  @param[in] enable  Enable or disable display timestamp.
 */
void engine::set_enable_timestamp(time_precision p) throw () {
  _show_timestamp = p;
}

/**
 *  Set thread id display.
 *
 *  @param[in] enable  Enable or disable display thread id.
 */
void engine::set_enable_thread_id(bool enable) throw () {
  _show_thread_id = enable;
}

/**
 *  Destroy the logging engine.
 */
void engine::unload() {
  delete _instance;
  _instance = NULL;
}

/**
 *  Default constructor.
 */
engine::engine()
  : _id(0),
    _show_pid(true),
    _show_timestamp(second),
    _show_thread_id(true) {
  memset(_list_verbose, 0, sizeof(_list_verbose));
}

/**
 *  Default copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
engine::engine(engine const& right) {
  _internal_copy(right);
}

/**
 *  Default destructor.
 */
engine::~engine() throw () {
  for (std::vector<backend_info*>::const_iterator
         it(_backends.begin()), end(_backends.end());
       it != end;
       ++it)
    delete *it;
}

/**
 *  Default copy operator.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
engine& engine::operator=(engine const& right) {
  return (_internal_copy(right));
}

/**
 *  Internal copy.
 *
 *  @param[in] right  The object to copy.
 *
 *  @return This object.
 */
engine& engine::_internal_copy(engine const& right) {
  (void)right;
  assert(!"impossible to copy logging::engine");
  abort();
  return (*this);
}

/**
 *  Rebuild the verbosities information.
 */
void engine::_rebuild_verbosities() {
  memset(_list_verbose, 0, sizeof(_list_verbose));
  for (std::vector<backend_info*>::const_iterator
         it(_backends.begin()), end(_backends.end());
       it != end;
       ++it) {
    for (unsigned int i(0); i < sizeof(type_flags) * CHAR_BIT; ++i)
      if (((*it)->types & (static_cast<type_flags>(1) << i))
          && _list_verbose[i] < (*it)->verbose)
        _list_verbose[i] = (*it)->verbose;
  }
}
