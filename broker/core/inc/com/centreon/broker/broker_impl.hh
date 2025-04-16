/**
 * Copyright 2020-2024 Centreon (https://www.centreon.com/)
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

#include <grpcpp/server_context.h>
#include "bbdo/events.hh"
#include "broker.grpc.pb.h"
#include "broker/core/src/broker.pb.h"
#include "com/centreon/broker/io/protobuf.hh"

namespace com::centreon::broker {

/**
 * Here is a declaration of pb_rebuild_graphs which is a bbdo event we use
 * to ask rebuild of metrics. MetricIds is a vector of metric ids to rebuild. */
namespace bbdo {
using pb_rebuild_graphs =
    io::protobuf<IndexIds, make_type(io::bbdo, bbdo::de_rebuild_graphs)>;
using pb_remove_graphs =
    io::protobuf<ToRemove, make_type(io::bbdo, bbdo::de_remove_graphs)>;
using pb_remove_poller =
    io::protobuf<GenericNameOrIndex,
                 make_type(io::bbdo, bbdo::de_remove_poller)>;
}  // namespace bbdo

namespace extcmd {
using pb_ba_info =
    io::protobuf<BaInfo, make_type(io::extcmd, extcmd::de_ba_info)>;
}

class broker_impl final : public Broker::Service {
  std::string _broker_name;

  grpc::Status GetVersion(grpc::ServerContext* context,
                          const ::google::protobuf::Empty* /*request*/,
                          Version* response) override;

  grpc::Status GetGenericStats(grpc::ServerContext* context,
                               const ::google::protobuf::Empty* request,
                               GenericString* response) override;

  grpc::Status GetSqlManagerStats(grpc::ServerContext* context,
                                  const SqlConnection* request,
                                  SqlManagerStats* response) override;
  grpc::Status GetConflictManagerStats(grpc::ServerContext* context,
                                       const ::google::protobuf::Empty* request,
                                       ConflictManagerStats* response) override;
  grpc::Status GetNumModules(grpc::ServerContext* context,
                             const ::google::protobuf::Empty* /*request*/,
                             GenericSize* response) override;

  grpc::Status GetModulesStats(grpc::ServerContext* context,
                               const GenericNameOrIndex* request,
                               GenericString* response) override;

  grpc::Status GetMuxerStats(grpc::ServerContext*,
                             const GenericString*,
                             MuxerStats*) override;

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

  grpc::Status GetBa(grpc::ServerContext* context,
                     const BaInfo* request,
                     ::google::protobuf::Empty* response) override;

  grpc::Status GetProcessingStats(grpc::ServerContext* context
                                  __attribute__((unused)),
                                  const ::google::protobuf::Empty* request
                                  __attribute__((unused)),
                                  ProcessingStats* response) override;
  grpc::Status RemovePoller(grpc::ServerContext* context
                            __attribute__((unused)),
                            const GenericNameOrIndex* request,
                            ::google::protobuf::Empty* response) override;
  grpc::Status GetLogInfo(grpc::ServerContext* context [[maybe_unused]],
                          const GenericString* request,
                          LogInfo* response) override;

  grpc::Status SetLogLevel(grpc::ServerContext* context [[maybe_unused]],
                           const LogLevel* request,
                           ::google::protobuf::Empty*) override;
  grpc::Status SetLogFlushPeriod(grpc::ServerContext* context [[maybe_unused]],
                                 const LogFlushPeriod* request,
                                 ::google::protobuf::Empty*) override;
  grpc::Status SetSqlManagerStats(grpc::ServerContext* context [[maybe_unused]],
                                  const SqlManagerStatsOptions* request,
                                  ::google::protobuf::Empty*) override;
  grpc::Status GetProcessStats(
      ::grpc::ServerContext* context,
      const ::google::protobuf::Empty* request,
      ::com::centreon::common::pb_process_stat* response) override;

  grpc::Status Aes256Encrypt(grpc::ServerContext* context,
                             const AesMessage* request,
                             GenericString* response) override;
  grpc::Status Aes256Decrypt(grpc::ServerContext* context,
                             const AesMessage* request,
                             GenericString* response) override;

  grpc::Status GetPeers(grpc::ServerContext* context,
                        const ::google::protobuf::Empty* request,
                        PeerList* response) override;

 public:
  broker_impl(const std::string& name);
  void set_broker_name(const std::string& s);
};
}  // namespace com::centreon::broker

#endif  // CENTREON_BROKER_CORE_SRC_BROKERIMPL_HH_
