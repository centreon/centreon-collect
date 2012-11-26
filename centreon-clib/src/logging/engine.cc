/*
** Copyright 2011-2012 Merethis
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

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>
#include "com/centreon/concurrency/locker.hh"
#include "com/centreon/exceptions/basic.hh"
#include "com/centreon/logging/engine.hh"

using namespace com::centreon::concurrency;
using namespace com::centreon::logging;

// Class instance.
static engine* _instance = NULL;

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
                        unsigned long long types,
                        unsigned int verbose) {
  if (!obj)
    throw (basic_error() << "add backend on the logging engine "
           "failed: bad argument (null pointer)");
  if (verbose >= sizeof(unsigned int) * CHAR_BIT)
    throw (basic_error() << "add backend on the logging engine "
           "failed: invalid verbose");

  std::auto_ptr<backend_info> info(new backend_info);
  info->obj = obj;
  info->types = types;
  info->verbose = verbose;

  // Lock engine.
  locker lock(&_mtx);
  info->id = ++_id;
  for (unsigned int i(0); i <= verbose; ++i)
    _list_types[i] |= types;

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
               unsigned long long types,
               unsigned int verbose) const throw () {
  if (verbose >= sizeof(unsigned int) * CHAR_BIT)
    return (false);

  // Lock engine.
  locker lock(&_mtx);
  return (_list_types[verbose] & types);
}

/**
 *  Create a new instance of the logging engine if no instance exist.
 */
void engine::load() {
  if (!_instance)
    _instance = new engine;
  return;
}

/**
 *  Log messages.
 *
 *  @param[in] types    The logging type to log.
 *  @param[in] verbose  The verbosity level.
 *  @param[in] msg      The string to log.
 *  @param[in] size     The string size to log.
 */
void engine::log(
               unsigned long long types,
               unsigned int verbose,
               char const* msg,
               unsigned int size) {
  if (!msg)
    return;

  // Lock engine.
  locker lock(&_mtx);
  for (std::vector<backend_info*>::const_iterator
         it(_backends.begin()), end(_backends.end());
       it != end;
       ++it)
    if (((*it)->types & types) && (*it)->verbose >= verbose)
      (*it)->obj->log(types, verbose, msg, size);
}

/**
 *  Remove backend by id.
 *
 *  @param[in] id  The backend id.
 *
 *  @return True if the backend was remove, otherwise false.
 */
bool engine::remove(unsigned long id) {
  // Lock engine.
  locker lock(&_mtx);
  for (std::vector<backend_info*>::iterator
         it(_backends.begin()), end(_backends.end());
       it != end;
       ++it)
    if ((*it)->id == id) {
      delete *it;
      _backends.erase(it);
      _rebuild_types();
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
    throw (basic_error() << "remove backend on the logging engine "
           "failed:bad argument (null pointer)");

  // Lock engine.
  locker lock(&_mtx);
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
    _rebuild_types();
  return (count_remove);
}

/**
 *  Close and open all backend.
 */
void engine::reopen() {
  locker lock(&_mtx);
  for (std::vector<backend_info*>::const_iterator
         it(_backends.begin()), end(_backends.end());
       it != end;
       ++it)
    (*it)->obj->reopen();
}

/**
 *  Destroy the logging engine.
 */
void engine::unload() {
  delete _instance;
  _instance = NULL;
  return;
}

/**
 *  Default constructor.
 */
engine::engine()
  : _id(0) {
  memset(_list_types, 0, sizeof(_list_types));
}

/**
 *  Copy constructor.
 *
 *  @param[in] right  The object to copy.
 */
engine::engine(engine const& right) {
  _internal_copy(right);
}

/**
 *  Destructor.
 */
engine::~engine() throw () {
  for (std::vector<backend_info*>::const_iterator
         it(_backends.begin()), end(_backends.end());
       it != end;
       ++it)
    delete *it;
}

/**
 *  Assignment operator.
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
 *  Rebuild the types information.
 */
void engine::_rebuild_types() {
  memset(_list_types, 0, sizeof(_list_types));
  for (std::vector<backend_info*>::const_iterator
         it(_backends.begin()), end(_backends.end());
       it != end;
       ++it) {
    for (unsigned int i(0); i <= (*it)->verbose; ++i)
      _list_types[i] |= (*it)->types;
  }
}
