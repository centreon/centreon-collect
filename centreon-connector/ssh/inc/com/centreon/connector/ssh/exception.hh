/*
** Copyright 2011 Merethis
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

#ifndef CCC_SSH_EXCEPTION_HH_
# define CCC_SSH_EXCEPTION_HH_

# include <exception>
# include <string>

namespace              com {
  namespace            centreon {
    namespace          connector {
      namespace        ssh {
        /**
         *  @class exception exception.hh "com/centreon/connector/ssh/exception.hh"
         *  @brief Exception class.
         *
         *  Exception class with a message.
         */
        class          exception : public std::exception {
         private:
          char         _buffer[2048];
          unsigned int _current;
          template     <typename T>
          exception&   _conversion(char const* format, T t) throw ();

         public:
                       exception();
                       exception(exception const& e);
                       ~exception() throw ();
          exception&   operator=(exception const& e);
          exception&   operator<<(char const* str) throw ();
          exception&   operator<<(std::string const& str) throw ();
          exception&   operator<<(int i) throw ();
          char const*  what() const throw ();
        };
      }
    }
  }
}

#endif /* !CCC_SSH_EXCEPTION_HH_ */
