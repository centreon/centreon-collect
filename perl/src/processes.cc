/*
** Copyright 2011 Merethis
** This file is part of Centreon Connector Perl.
**
** Centreon Connector Perl is free software: you can redistribute it
** and/or modify it under the terms of the GNU Affero General Public
** License as published by the Free Software Foundation, either version
** 3 of the License, or (at your option) any later version.
**
** Centreon Connector Perl is distributed in the hope that it will be
** useful, but WITHOUT ANY WARRANTY; without even the implied warranty
** of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public
** License along with Centreon Connector Perl. If not, see
** <http://www.gnu.org/licenses/>.
*/

#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include "com/centreon/connector/perl/processes.hh"

using namespace com::centreon::connector::perl;

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
processes::processes() : _pid(getpid()) {}

/**
 *  Copy constructor.
 *
 *  @param[in] p Object to copy.
 */
processes::processes(processes const& p) {
  (void)p;
  assert(false);
  abort();
}

/**
 *  Assignment operator.
 *
 *  @param[in] p Object to copy.
 *
 *  @return This object.
 */
processes& processes::operator=(processes const& p) {
  (void)p;
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
processes::~processes() {
  if (getpid() == _pid) {
    // Send SIGKILL to all processes.
    for (std::map<pid_t, process*>::iterator
           it = _set.begin(), end = _set.end();
         it != end;
         ++it) {
      kill(it->first, SIGKILL);
      delete it->second;
      it->second = NULL;
    }

    // Wait for all children to terminate.
    int status;
    for (std::map<pid_t, process*>::iterator
           it = _set.begin(), end = _set.end();
         it != end;
         ++it)
      waitpid(it->first, &status, 0);
  }
}

/**
 *  Operator to access or insert data in the process set.
 *
 *  @param[in] key Key value.
 *
 *  @return Reference to associated value.
 */
process*& processes::operator[](pid_t key) {
  return (_set[key]);
}

/**
 *  Get an iterator to the beginning of the process set.
 *
 *  @return First iterator of the set.
 */
std::map<pid_t, process*>::iterator processes::begin() {
  return (_set.begin());
}

/**
 *  Check if the process list is empty.
 *
 *  @return true if the process list is empty.
 */
bool processes::empty() const {
  return (_set.empty());
}

/**
 *  Get an iterator to the end of the process set.
 *
 *  @return Last iterator of the set.
 */
std::map<pid_t, process*>::iterator processes::end() {
  return (_set.end());
}

/**
 *  Erase a value from the set.
 *
 *  @param[in] key Key to erase.
 */
void processes::erase(pid_t key) {
  std::map<pid_t, process*>::iterator it(_set.find(key));
  if (it != _set.end()) {
    delete it->second;
    it->second = NULL;
    _set.erase(it);
  }
  return ;
}

/**
 *  Get the class instance.
 *
 *  @return Class instance.
 */
processes& processes::instance() {
  static processes gl;
  return (gl);
}

/**
 *  Get the process list size.
 *
 *  @return Process list size.
 */
unsigned int processes::size() const {
  return (_set.size());
}
