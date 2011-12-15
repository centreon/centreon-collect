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

#ifndef CC_LOGGING_LOGGER_HH
#  define CC_LOGGING_LOGGER_HH

#  include <string>
#  include "com/centreon/logging/temp_logger.hh"
#  include "com/centreon/namespace.hh"

CC_BEGIN()

namespace       logging {
  enum          type_value {
    type_info = 0,
    type_debug = 1,
    type_error = 2
  };

  enum          verbosity_level {
    none = 0,
    low = 1,
    medium = 2,
    high = 3
  };

  /**
   *  @class logger logger.hh "com/centreon/logging/logger.hh"
   *  @brief Log messages.
   *
   *  This class provide basic logger (info, debug, error).
   */
  class         logger {
  public:
                logger(
                  type_number type,
                  char const* prefix = NULL) throw ();
                logger(logger const& right);
                ~logger() throw ();
    logger&     operator=(logger const& right);
    temp_logger operator()(verbosity const& verbose) const;
    temp_logger operator()(verbosity_level level) const;

  private:
    logger&     _internal_copy(logger const& right);

    std::string _prefix;
    type_number _type;
  };

  extern logger info;
  extern logger debug;
  extern logger error;
}

CC_END()

#endif // !CC_LOGGING_LOGGER_HH
