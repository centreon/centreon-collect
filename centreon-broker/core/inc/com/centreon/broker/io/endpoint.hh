/*
** Copyright 2011-2012 Centreon
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

#ifndef CCB_IO_ENDPOINT_HH
#  define CCB_IO_ENDPOINT_HH

#  include <QString>
#  include <string>
#  include <set>
#  include "com/centreon/broker/io/properties.hh"
#  include "com/centreon/broker/io/stream.hh"
#  include "com/centreon/broker/misc/shared_ptr.hh"
#  include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

// Forward declaration.
class                                persistent_cache;

namespace                            io {
  /**
   *  @class endpoint endpoint.hh "com/centreon/broker/io/endpoint.hh"
   *  @brief Base class of connectors and acceptors.
   *
   *  Endpoint are used to open data streams. Endpoints can be either
   *  acceptors (which wait for incoming connections) or connectors
   *  (that initiate connections).
   */
  class                              endpoint {
   public:
                                     endpoint(bool is_accptr);
                                     endpoint(endpoint const& other);
    virtual                          ~endpoint();
    endpoint&                        operator=(endpoint const& other);
    void                             from(
                                       misc::shared_ptr<endpoint> endp);
    bool                             is_acceptor() const throw ();
    bool                             is_connector() const throw ();
    virtual misc::shared_ptr<stream> open() = 0;
    virtual void                     stats(io::properties& tree);
    void                             set_filter(
                                       std::set<unsigned int> const& filter);

   protected:
    void                             _internal_copy(
                                       endpoint const& other);

    misc::shared_ptr<endpoint>       _from;
    bool                             _is_acceptor;
    std::set<unsigned int>           _filter;
  };
}

CCB_END()

#endif // !CCB_IO_ENDPOINT_HH
