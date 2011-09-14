/*
** Copyright 2011 Merethis
**
** This file is part of Centreon Connector SSH.
**
** Centreon Connector SSH is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector SSH is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector SSH. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <stdlib.h>
#include "com/centreon/connector/ssh/session.hh"
#include "com/centreon/connector/ssh/sessions.hh"

using namespace com::centreon::connector::ssh;

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Constructor.
 */
sessions::sessions() {}

/**
 *  @brief Copy constructor.
 *
 *  Any call to this constructor will result in a call to abort().
 *
 *  @param[in] s Object to copy.
 */
sessions::sessions(sessions const& s) {
  (void)s;
  assert(false);
  abort();
}

/**
 *  @brief Assignment operator.
 *
 *  Any call to this method will result in a call to abort().
 *
 *  @param[in] s Object to copy.
 *
 *  @return This object.
 */
sessions& sessions::operator=(sessions const& s) {
  (void)s;
  assert(false);
  abort();
  return (*this);
}

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Destructor.
 */
sessions::~sessions() {
  // Delete all sessions.
  for (std::map<credentials, session*>::iterator
         it = _set.begin(),
         end = _set.end();
       it != end;
       ++it)
    try {
      delete it->second;
    }
    catch (...) {}
}

/**
 *  Access a session by credential.
 *
 *  @return Reference to the associated session pointer.
 */
session*& sessions::operator[](credentials const& c) {
  return (_set[c]);
}

/**
 *  Get an iterator to the beginning of the session set.
 *
 *  @return First iterator.
 */
std::map<credentials, session*>::iterator sessions::begin() {
  return (_set.begin());
}

/**
 *  Check if the session set is empty.
 *
 *  @return true if the session set is empty.
 */
bool sessions::empty() const {
  return (_set.empty());
}

/**
 *  Get an iterator to the end of the session set.
 *
 *  @return Last iterator.
 */
std::map<credentials, session*>::iterator sessions::end() {
  return (_set.end());
}

/**
 *  Erase a session.
 *
 *  @param[in] key Element key.
 */
void sessions::erase(credentials const& key) {
  std::map<credentials, session*>::iterator it(_set.find(key));
  if (it != _set.end()) {
    try {
      delete it->second;
    }
    catch (...) {}
    _set.erase(it);
  }
  return ;
}

/**
 *  Erase a session by its iterator.
 *
 *  @param[in] it Iterator to the session.
 */
void sessions::erase(std::map<credentials, session*>::iterator it) {
  try {
    delete it->second;
  }
  catch (...) {}
  _set.erase(it);
  return ;
}

/**
 *  Get the class instance.
 *
 *  @return Class instance.
 */
sessions& sessions::instance() {
  static sessions gl;
  return (gl);
}

/**
 *  Get the number of registered sessions.
 *
 *  @return Number of registered sessions.
 */
unsigned int sessions::size() const {
  return (_set.size());
}
