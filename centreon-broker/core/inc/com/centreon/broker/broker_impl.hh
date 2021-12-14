/*
 * Copyright 2020 Centreon (https://www.centreon.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */
#ifndef CENTREON_BROKER_CORE_SRC_BROKERIMPL_HH_
#define CENTREON_BROKER_CORE_SRC_BROKERIMPL_HH_

#include "bbdo/events.hh"
#include "broker.grpc.pb.h"
#include "centreon-broker/core/src/broker.pb.h"
#include "com/centreon/broker/io/protobuf.hh"
#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

/**
 * Here is a declaration of pb_rebuild_rrd_graphs which is a bbdo event we use
 * to ask rebuild of metrics. MetricIds is a vector of metric ids to rebuild. */
namespace bbdo {
using pb_rebuild_rrd_graphs =
    io::protobuf<IndexIds, make_type(io::bbdo, bbdo::de_rebuild_rrd_graphs)>;
using pb_remove_graphs =
    io::protobuf<ToRemove, make_type(io::bbdo, bbdo::de_remove_graphs)>;
}

class broker_impl final : public Broker::Service {
  std::string _broker_name;

  grpc::Status GetVersion(grpc::ServerContext* context,
                          const ::google::protobuf::Empty* /*request*/,
                          Version* response) override;

  grpc::Status GetGenericStats(grpc::ServerContext* context,
                               const ::google::protobuf::Empty* request,
                               GenericString* response) override;

  grpc::Status GetSqlConnectionStats(grpc::ServerContext* context,
                                     const GenericInt* request,
                                     SqlConnectionStats* response) override;
  grpc::Status GetConflictManagerStats(grpc::ServerContext* context,
                                       const ::google::protobuf::Empty* request,
                                       ConflictManagerStats* response) override;
  grpc::Status GetSqlConnectionSize(grpc::ServerContext* context,
                                    const ::google::protobuf::Empty* request,
                                    GenericSize* response) override;
  grpc::Status GetNumModules(grpc::ServerContext* context,
                             const ::google::protobuf::Empty* /*request*/,
                             GenericSize* response) override;

  grpc::Status GetModulesStats(grpc::ServerContext* context,
                               const GenericNameOrIndex* request,
                               GenericString* response) override;

  grpc::Status GetNumEndpoint(grpc::ServerContext* context,
                              const ::google::protobuf::Empty* /*request*/,
                              GenericSize* response) override;

  grpc::Status GetEndpointStats(grpc::ServerContext* context,
                                const GenericNameOrIndex* request,
                                GenericString* response) override;

  grpc::Status RebuildRRDGraphs(grpc::ServerContext* context,
                                const IndexIds* request,
                                ::google::protobuf::Empty* response) override;

  grpc::Status RemoveGraphs(grpc::ServerContext* context,
                                const ToRemove* request,
                                ::google::protobuf::Empty* response) override;

 public:
  void set_broker_name(std::string const& s) { _broker_name = s; };
};
CCB_END()

#endif  // CENTREON_BROKER_CORE_SRC_BROKERIMPL_HH_
