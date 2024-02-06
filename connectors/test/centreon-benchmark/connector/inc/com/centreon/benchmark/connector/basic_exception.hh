/*
** Copyright 2011-2013 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCB_CONNECTOR_BASIC_EXCEPTION
#define CCB_CONNECTOR_BASIC_EXCEPTION

#include <exception>

CCB_CONNECTOR_BEGIN()

/**
 *  @class basic_exception basic_exception.hh
 *"com/centreon/benchmark/connector/basic_exception.hh"
 *  @brief Base exception class.
 *
 *  Simple exception class containing an basic error message.
 */
class basic_exception : public std::exception {
 public:
  basic_exception(char const* message = "");
  basic_exception(basic_exception const& right);
  ~basic_exception() throw();
  basic_exception& operator=(basic_exception const& right);
  char const* what() const throw();

 private:
  basic_exception& _internal_copy(basic_exception const& right);

  char const* _message;
};

CCB_CONNECTOR_END()

#endif  // !CCB_CONNECTOR_BASIC_EXCEPTION
