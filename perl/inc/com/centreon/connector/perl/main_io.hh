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

#ifndef CCC_PERL_MAIN_IO_HH_
#define CCC_PERL_MAIN_IO_HH_

#include <string>
#include "com/centreon/connector/perl/namespace.hh"

CCC_PERL_BEGIN()

/**
 *  @class main_io main_io.hh
 *  @brief Manage I/O on standard input and standard output.
 *
 *  This singleton manages I/O on standard input (wait for
 *  orders from the monitoring engine) and on standard output
 *  (provide check results).
 */
class main_io {
 public:
  ~main_io();
  static main_io& instance();
  int read();
  int write();
  void write(std::string const& data);
  bool write_wanted() const;

 private:
  main_io();
  main_io(main_io const& mio);
  main_io& operator=(main_io const& mio);
  int _parse(std::string const& cmd);

  std::string _rbuffer;
  std::string _wbuffer;
};

CCC_PERL_END()

#endif /* !CCC_PERL_MAIN_IO_HH_ */
