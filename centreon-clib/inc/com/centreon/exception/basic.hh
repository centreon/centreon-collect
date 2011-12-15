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

#ifndef CC_EXCEPTION_BASIC_HH
#  define CC_EXCEPTION_BASIC_HH

#  include <exception>
#  include <string>
#  include "com/centreon/namespace.hh"
#  include "com/centreon/misc/stringifier.hh"

CC_BEGIN()

namespace               exception {
  /**
   *  @class basic basic.hh "com/centreon/concurrency/basic.hh"
   *  @brief Base exception class.
   *
   *  Simple exception class containing an basic error message.
   */
  class                 basic : public std::exception {
  public:
                        basic() throw();
                        basic(
                          char const* file,
                          char const* function,
                          int line) throw ();
                        basic(basic const& right) throw ();
    virtual             ~basic() throw ();
    virtual basic&      operator=(basic const& right) throw ();
    template<typename T>
    basic&              operator<<(T t) throw () {
      _buffer << t;
      return (*this);
    }
    virtual char const* what() const throw ();

  private:
    basic&              _internal_copy(basic const& right);

    misc::stringifier   _buffer;
  };
}

CC_END()

#  ifdef __GNUC__
#    define FUNCTION __PRETTY_FUNCTION__
#  else
#    define FUNCTION __func__
#  endif

#  ifndef NDEBUG
#    define basic_error() com::centreon::exception::basic( \
                          __FILE__,                        \
                          FUNCTION,                        \
                          __LINE__)
#  else
#    define basic_error() com::centreon::exception::basic()
#  endif // !NDEBUG
#endif // !CC_EXCEPTION_BASIC_HH
