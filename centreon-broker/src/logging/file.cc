/*
**  Copyright 2010 MERETHIS
**  This file is part of CentreonBroker.
**
**  CentreonBroker is free software: you can redistribute it and/or modify it
**  under the terms of the GNU General Public License as published by the Free
**  Software Foundation, either version 2 of the License, or (at your option)
**  any later version.
**
**  CentreonBroker is distributed in the hope that it will be useful, but
**  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
**  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
**  for more details.
**
**  You should have received a copy of the GNU General Public License along
**  with CentreonBroker.  If not, see <http://www.gnu.org/licenses/>.
**
**  For more information : contact@centreon.com
*/

#include <assert.h>
#include "exceptions/basic.hh"
#include "logging/file.hh"

using namespace logging;

/**************************************
*                                     *
*           Private Methods           *
*                                     *
**************************************/

/**
 *  Copy constructor.
 *
 *  @param[in] f Unused.
 */
file::file(file const& f) : ostream(f)
{
  (void)f;
  assert(false);
}

/**
 *  Assignment operator overload.
 *
 *  @param[in] f Unused.
 *
 *  @return Current instance.
 */
file& file::operator=(file const& f)
{
  (void)f;
  assert(false);
  return (*this);
}

/**************************************
*                                     *
*           Public Methods            *
*                                     *
**************************************/

/**
 *  Default constructor.
 */
file::file() {}

/**
 *  Constructor.
 *
 *  @param[in] filename Name of the file to open.
 */
file::file(char const* filename)
{
  open(filename);
}

/**
 *  Destructor.
 */
file::~file() {}

/**
 *  Open a log file.
 *
 *  @param[in] filename Name of the file to open.
 *
 *  @throw exceptions::basic Could not open file.
 */
void file::open(char const* filename)
{
  // Check filename is not NULL.
  if (!filename)
    throw (exceptions::basic() << "Trying to open NULL log file");

  // Close file if previously opened.
  if (_ofs.is_open())
    _ofs.close();

  // Open file.
  _ofs.open(filename, std::ios_base::out
                      | std::ios_base::app
                      | std::ios_base::ate);
  if (_ofs.fail())
    throw (exceptions::basic() << "Could not open \""
                               << filename << "\" log file");

  // Don't forget to set the stream to write on.
  ostream::operator=(_ofs);

  return ;
}
