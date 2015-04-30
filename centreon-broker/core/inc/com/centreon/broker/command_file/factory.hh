/*
** Copyright 2013 Merethis
**
** This file is part of Centreon Broker.
**
** Centreon Broker is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Broker is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Broker. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCB_COMMAND_FILE_FACTORY_HH
#  define CCB_COMMAND_FILE_FACTORY_HH

#  include "com/centreon/broker/io/factory.hh"
#  include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace         command_file {
  /**
   *  @class factory factory.hh "com/centreon/broker/command_file/factory.hh"
   *  @brief Command file factory.
   *
   *  Build Command file endpoints.
   */
  class           factory : public io::factory {
  public:
                  factory();
                  factory(factory const& right);
                  ~factory();
    factory&      operator=(factory const& right);
    io::factory*  clone() const;
    bool          has_endpoint(
                    config::endpoint& cfg,
                    bool is_input,
                    bool is_output) const;
    io::endpoint* new_endpoint(
                    config::endpoint& cfg,
                    bool is_input,
                    bool is_output,
                    bool& is_acceptor,
                    misc::shared_ptr<persistent_cache> cache
                      = misc::shared_ptr<persistent_cache>()) const;
  };
}

CCB_END()

#endif // !CCB_COMMAND_FILE_FACTORY_HH
