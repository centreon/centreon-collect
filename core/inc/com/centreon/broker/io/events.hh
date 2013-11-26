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

#ifndef CCB_IO_EVENTS_HH
#  define CCB_IO_EVENTS_HH

#  include <map>
#  include <set>
#  include <string>
#  include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace               io {
  /**
   *  @class events events.hh "com/centreon/broker/io/events.hh"
   *  @brief Data events registration.
   *
   *  Maintain the set of existing events.
   */
  class                 events {
  public:
    enum                data_category {
      neb = 1,
      bbdo,
      storage,
      correlation,
      dumper,
      internal = 65535
    };

    template <unsigned short category, unsigned short element>
    struct data_type {
      enum { value = (category << 16 | element) };
    };

    std::map<std::string, std::set<unsigned int> >::const_iterator
                        begin() const;
    static unsigned short
                        category_of_type(unsigned int type) throw () {
      return (static_cast<unsigned short>(type >> 16));
    }
    static unsigned short
                        element_of_type(unsigned int type) throw () {
      return (static_cast<unsigned short>(type));
    }
    std::map<std::string, std::set<unsigned int> >::const_iterator
                        end() const;
    std::set<unsigned int> const&
                        get(std::string const& name) const;
    static events&      instance();
    static void         load();
    void                reg(
                          std::string const& name,
                          std::set<unsigned int> const& elems);
    static void         unload();
    void                unreg(std::string const& name);

  private:
                        events();
                        events(events const& right);
                        ~events();
    events&             operator=(events const& right);

    std::map<std::string, std::set<unsigned int> >
                        _elements;
  };
}

CCB_END()

#endif // !CCB_IO_EVENTS_HH
