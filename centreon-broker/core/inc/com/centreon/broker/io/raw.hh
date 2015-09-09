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

#ifndef CCB_IO_RAW_HH
#  define CCB_IO_RAW_HH

#  include <QByteArray>
#  include "com/centreon/broker/io/data.hh"
#  include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace          io {
  /**
   *  @class raw raw.hh "io/raw.hh"
   *  @brief Raw byte array.
   *
   *  Raw byte array.
   */
  class            raw : public data, public QByteArray {
  public:
                   raw();
                   raw(raw const& r);
                   ~raw();
    raw&           operator=(raw const& r);
    unsigned int   type() const;
    static unsigned int
                   static_type();
  };
}

CCB_END()

#endif // !CCB_IO_RAW_HH
