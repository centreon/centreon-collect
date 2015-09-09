/*
** Copyright 2014 Centreon
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

#ifndef CCB_BAM_REBUILD_HH
#  define CCB_BAM_REBUILD_HH

#  include <QString>
#  include "com/centreon/broker/io/data.hh"
#  include "com/centreon/broker/namespace.hh"
#  include "com/centreon/broker/io/event_info.hh"
#  include "com/centreon/broker/mapping/entry.hh"

CCB_BEGIN()

namespace          bam {
  /**
   *  @class rebuild rebuild.hh "com/centreon/broker/bam/rebuild.hh"
   *  @brief ask for a rebuild.
   *
   *  This data event represent a rebuild asked.
   */
  class            rebuild : public io::data {
  public:
                   rebuild();
                   ~rebuild();
                   rebuild(rebuild const&);
    rebuild&       operator=(rebuild const&);
    bool           operator==(rebuild const& other) const;
    unsigned int   type() const;
    static unsigned int
                   static_type();

    QString        bas_to_rebuild;

    static mapping::entry const
                   entries[];
    static io::event_info::event_operations const
                   operations;

  private:
    void           _internal_copy(rebuild const& other);
  };
}

CCB_END()

#endif // !CCB_BAM_REBUILD_HH
