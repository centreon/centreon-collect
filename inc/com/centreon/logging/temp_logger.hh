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

#ifndef CC_LOGGING_TEMP_LOGGER_HH
#  define CC_LOGGING_TEMP_LOGGER_HH

#  include <string>
#  include "com/centreon/logging/engine.hh"
#  include "com/centreon/logging/verbosity.hh"
#  include "com/centreon/namespace.hh"
#  include "com/centreon/misc/stringifier.hh"

CC_BEGIN()

namespace                     logging {
    struct                    setprecision {
                              setprecision(int val = -1)
                                : precision(val) {}
      int                     precision;
    };

  /**
   *  @class temp_logger temp_logger.hh "com/centreon/logging/temp_logger.hh"
   *  @brief Log messages.
   *
   *  Used to buffering log messages before writing them into backends.
   */
  class                       temp_logger {
  public:
                              temp_logger(
                                type_number type,
                                verbosity verbose) throw ();
                              temp_logger(temp_logger const& right);
    virtual                   ~temp_logger() throw ();
    temp_logger&              operator=(temp_logger const& right);
    temp_logger&              operator<<(char const* obj) throw ();
    temp_logger&              operator<<(char obj) throw ();
    temp_logger&              operator<<(double obj) throw ();
    temp_logger&              operator<<(int obj) throw ();
    temp_logger&              operator<<(long long obj) throw ();
    temp_logger&              operator<<(long obj) throw ();
    temp_logger&              operator<<(setprecision const& obj) throw ();
    temp_logger&              operator<<(std::string const& obj) throw ();
    temp_logger&              operator<<(unsigned int obj) throw ();
    temp_logger&              operator<<(unsigned long long obj) throw ();
    temp_logger&              operator<<(unsigned long obj) throw ();
    temp_logger&              operator<<(void const* obj) throw ();

  private:
    struct                    redirector {
      temp_logger&            (temp_logger::*
                               redir_char)(char) throw ();
      temp_logger&            (temp_logger::*
                               redir_double)(double) throw ();
      temp_logger&            (temp_logger::*
                               redir_int)(int) throw ();
      temp_logger&            (temp_logger::*
                               redir_flush)() throw ();
      temp_logger&            (temp_logger::*
                               redir_long_long)(long long) throw ();
      temp_logger&            (temp_logger::*
                               redir_long)(long) throw ();
      temp_logger&            (temp_logger::*
                               redir_setprecision)(setprecision const&) throw ();
      temp_logger&            (temp_logger::*
                               redir_std_string)(std::string const&) throw ();
      temp_logger&            (temp_logger::*
                               redir_string)(char const*) throw ();
      temp_logger&            (temp_logger::*
                               redir_uint)(unsigned int) throw ();
      temp_logger&            (temp_logger::*
                               redir_ulong_long)(unsigned long long) throw ();
      temp_logger&            (temp_logger::*
                               redir_ulong)(unsigned long) throw ();
      temp_logger&            (temp_logger::*
                               redir_void_ptr)(void const*) throw ();
    };

    template<typename T>
    temp_logger&              _builder(T obj) throw ();
    temp_logger&              _builder_setprecision(
                                setprecision const& obj) throw ();
    temp_logger&              _internal_copy(temp_logger const& right);
    temp_logger&              _flush() throw ();
    template<typename T>
    temp_logger&              _nothing(T obj) throw ();
    temp_logger&              _nothing() throw ();

    misc::stringifier         _buffer;
    engine&                   _engine;
    mutable redirector const* _redirector;
    static redirector const   _redir_builder;
    static redirector const   _redir_nothing;
    type_number               _type;
    verbosity                 _verbose;
  };
}

CC_END()

#endif // !CC_LOGGING_TEMP_LOGGER_HH
