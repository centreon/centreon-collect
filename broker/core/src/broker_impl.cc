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

#include "com/centreon/broker/broker_impl.hh"
#include <google/protobuf/util/time_util.h>
#include <grpcpp/support/status.h>

#include "com/centreon/broker/config/applier/endpoint.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/broker/stats/helper.hh"
#include "com/centreon/broker/version.hh"
#include "com/centreon/common/process_stat.hh"
#include "common/crypto/aes256.hh"
#include "common/log_v2/log_v2.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::version;
using com::centreon::common::crypto::aes256;
using com::centreon::common::log_v2::log_v2;

broker_impl::broker_impl(const std::string& name) : _broker_name(name) {}

/**
 * @brief Return the Broker's version.
 *
 * @param context gRPC context
 * @param  unused
 * @param response A Version object to fill
 *
 * @return Status::OK
 */
grpc::Status broker_impl::GetVersion(grpc::ServerContext* context
                                     [[maybe_unused]],
                                     const ::google::protobuf::Empty* request
                                     [[maybe_unused]],
                                     Version* response) {
  response->set_major(major);
  response->set_minor(minor);
  response->set_patch(patch);
  return grpc::Status::OK;
}

grpc::Status broker_impl::GetNumModules(grpc::ServerContext* context
                                        [[maybe_unused]],
                                        const ::google::protobuf::Empty*,
                                        GenericSize* response) {
  auto& mod_applier(config::applier::state::instance().get_modules());

  std::lock_guard<std::mutex> lock(mod_applier.module_mutex());
  response->set_size(std::distance(mod_applier.begin(), mod_applier.end()));

  return grpc::Status::OK;
}

grpc::Status broker_impl::GetNumEndpoint(grpc::ServerContext* context
                                         [[maybe_unused]],
                                         const ::google::protobuf::Empty*,
                                         GenericSize* response) {
  // Endpoint applier.
  config::applier::endpoint& endp_applier(
      config::applier::endpoint::instance());

  std::lock_guard<std::timed_mutex> lock(endp_applier.endpoints_mutex());
  response->set_size(std::distance(endp_applier.endpoints_begin(),
                                   endp_applier.endpoints_end()));

  return grpc::Status::OK;
}

grpc::Status broker_impl::GetModulesStats(grpc::ServerContext* context
                                          [[maybe_unused]],
                                          const GenericNameOrIndex* request,
                                          GenericString* response) {
  std::vector<nlohmann::json> value;
  stats::get_loaded_module_stats(value);

  bool found{false};
  nlohmann::json object;
  switch (request->nameOrIndex_case()) {
    case GenericNameOrIndex::NAMEORINDEX_NOT_SET:
      for (auto& obj : value) {
        object["module" + obj["name"].get<std::string>()] = obj;
      }
      response->set_str_arg(object.dump());
      break;

    case GenericNameOrIndex::kStr:
      for (auto& obj : value) {
        if (obj["name"].get<std::string>() == request->str()) {
          found = true;
          response->set_str_arg(object.dump());
          break;
        }
      }
      if (!found)
        return grpc::Status(grpc::INVALID_ARGUMENT,
                            grpc::string("name not found"));

      break;

    case GenericNameOrIndex::kIdx:

      if (request->idx() + 1 > value.size())
        return grpc::Status(grpc::INVALID_ARGUMENT,
                            grpc::string("idx too big"));

      object = value[request->idx()];
      response->set_str_arg(object.dump());
      break;

    default:
      return grpc::Status::CANCELLED;
      break;
  }

  return grpc::Status::OK;
}

grpc::Status broker_impl::GetEndpointStats(grpc::ServerContext* context
                                           [[maybe_unused]],
                                           const GenericNameOrIndex* request,
                                           GenericString* response) {
  std::vector<nlohmann::json> value;
  try {
    if (!stats::get_endpoint_stats(value))
      return grpc::Status(grpc::UNAVAILABLE, grpc::string("endpoint locked"));
  } catch (...) {
    return grpc::Status(grpc::ABORTED, grpc::string("endpoint throw error"));
  }

  bool found{false};
  nlohmann::json object;

  switch (request->nameOrIndex_case()) {
    case GenericNameOrIndex::NAMEORINDEX_NOT_SET:
      for (auto& obj : value) {
        object["module" + obj["name"].get<std::string>()] = obj;
      }
      response->set_str_arg(object.dump());
      break;

    case GenericNameOrIndex::kStr:
      for (auto& obj : value) {
        if (obj["name"].get<std::string>() == request->str()) {
          found = true;
          response->set_str_arg(obj.dump());
          break;
        }
      }
      if (!found)
        return grpc::Status(grpc::INVALID_ARGUMENT,
                            grpc::string("name not found"));
      break;

    case GenericNameOrIndex::kIdx:

      if ((request->idx() + 1) > value.size())
        return grpc::Status(grpc::INVALID_ARGUMENT,
                            grpc::string("idx too big"));

      object = value[request->idx()];
      response->set_str_arg(object.dump());
      break;

    default:
      return grpc::Status::CANCELLED;
      break;
  }
  return grpc::Status::OK;
}

grpc::Status broker_impl::GetGenericStats(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    GenericString* response) {
  nlohmann::json object;
  stats::get_generic_stats(object);

  response->set_str_arg(object.dump());
  return grpc::Status::OK;
}

grpc::Status broker_impl::GetSqlManagerStats(grpc::ServerContext* context
                                             [[maybe_unused]],
                                             const SqlConnection* request,
                                             SqlManagerStats* response) {
  auto center = config::applier::state::instance().center();
  if (!request->has_id())
    center->get_sql_manager_stats(response);
  else {
    try {
      center->get_sql_manager_stats(response, request->id());
    } catch (const std::exception& e) {
      return grpc::Status(grpc::StatusCode::NOT_FOUND, e.what());
    }
  }
  return grpc::Status::OK;
}

grpc::Status broker_impl::SetSqlManagerStats(
    grpc::ServerContext* context [[maybe_unused]],
    const SqlManagerStatsOptions* request,
    ::google::protobuf::Empty*) {
  auto& conf = config::applier::state::mut_stats_conf();

  if (request->has_slowest_statements_count())
    conf.sql_slowest_statements_count = request->slowest_statements_count();
  if (request->has_slowest_queries_count())
    conf.sql_slowest_queries_count = request->slowest_queries_count();

  return grpc::Status::OK;
}

grpc::Status broker_impl::GetConflictManagerStats(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    ConflictManagerStats* response) {
  config::applier::state::instance().center()->get_conflict_manager_stats(
      response);
  return grpc::Status::OK;
}

grpc::Status broker_impl::GetMuxerStats(grpc::ServerContext* context
                                        [[maybe_unused]],
                                        const GenericString* request,
                                        MuxerStats* response) {
  const std::string name = request->str_arg();
  bool status =
      config::applier::state::instance().center()->muxer_stats(name, response);
  return status ? grpc::Status::OK
                : grpc::Status(
                      grpc::StatusCode::NOT_FOUND,
                      fmt::format("no muxer stats found for name '{}'", name));
}

void broker_impl::set_broker_name(const std::string& s) {
  _broker_name = s;
}

/**
 * @brief The internal part of the gRPC RebuildMetrics() function.
 *
 * @param context (unused)
 * @param request A pointer to a MetricIds which contains a vector of metric
 * ids. These ids correspond to the metrics to rebuild.
 * @param response (unused)
 *
 * @return grpc::Status::OK
 */
grpc::Status broker_impl::RebuildRRDGraphs(grpc::ServerContext* context
                                           [[maybe_unused]],
                                           const IndexIds* request,
                                           ::google::protobuf::Empty* response
                                           [[maybe_unused]]) {
  multiplexing::publisher pblshr;
  auto e{std::make_shared<bbdo::pb_rebuild_graphs>(*request)};
  pblshr.write(e);
  return grpc::Status::OK;
}

grpc::Status broker_impl::RemoveGraphs(grpc::ServerContext* context
                                       [[maybe_unused]],
                                       const ToRemove* request,
                                       ::google::protobuf::Empty* response
                                       [[maybe_unused]]) {
  multiplexing::publisher pblshr;
  auto e{std::make_shared<bbdo::pb_remove_graphs>(*request)};
  pblshr.write(e);
  return grpc::Status::OK;
}

grpc::Status broker_impl::GetBa(grpc::ServerContext* context [[maybe_unused]],
                                const BaInfo* request,
                                ::google::protobuf::Empty* response
                                [[maybe_unused]]) {
  multiplexing::publisher pblshr;
  auto e{std::make_shared<extcmd::pb_ba_info>(*request)};
  pblshr.write(e);
  return grpc::Status::OK;
}

grpc::Status broker_impl::GetProcessingStats(
    grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    ::ProcessingStats* response) {
  config::applier::state::instance().center()->get_processing_stats(response);
  return grpc::Status::OK;
}

grpc::Status broker_impl::RemovePoller(grpc::ServerContext* context
                                       [[maybe_unused]],
                                       const GenericNameOrIndex* request,
                                       ::google::protobuf::Empty*) {
  log_v2::instance().get(log_v2::CORE)->info("Remove poller...");
  multiplexing::publisher pblshr;
  auto e{std::make_shared<bbdo::pb_remove_poller>(*request)};
  pblshr.write(e);
  return grpc::Status::OK;
}

grpc::Status broker_impl::GetLogInfo(grpc::ServerContext* context
                                     [[maybe_unused]],
                                     const GenericString* request,
                                     LogInfo* response) {
  auto& name{request->str_arg()};
  auto& map = *response->mutable_level();
  auto lvs = log_v2::instance().levels();
  response->set_log_name(log_v2::instance().log_name());
  response->set_log_file(log_v2::instance().filename());
  response->set_log_flush_period(log_v2::instance().flush_interval().count());
  if (!name.empty()) {
    auto found = std::find_if(
        lvs.begin(), lvs.end(),
        [&name](std::pair<std::string, spdlog::level::level_enum>& p) {
          return p.first == name;
        });
    if (found != lvs.end()) {
      auto level = to_string_view(found->second);
      map[name] = std::string(level.data(), level.size());
      return grpc::Status::OK;
    } else {
      std::string msg{fmt::format("'{}' is not a logger in broker", name)};
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, msg);
    }
  } else {
    for (auto& p : lvs) {
      auto level = to_string_view(p.second);
      map[p.first] = std::string(level.data(), level.size());
    }
    return grpc::Status::OK;
  }
}

grpc::Status broker_impl::SetLogLevel(grpc::ServerContext* context
                                      [[maybe_unused]],
                                      const LogLevel* request,
                                      ::google::protobuf::Empty*) {
  const std::string& logger_name{request->logger()};
  std::shared_ptr<spdlog::logger> logger = spdlog::get(logger_name);
  if (!logger) {
    std::string err_detail =
        fmt::format("The '{}' logger does not exist", logger_name);
    SPDLOG_LOGGER_ERROR(log_v2::instance().get(log_v2::CORE), err_detail);
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, err_detail);
  } else {
    logger->set_level(spdlog::level::level_enum(request->level()));
    return grpc::Status::OK;
  }
}

grpc::Status broker_impl::SetLogFlushPeriod(grpc::ServerContext* context
                                            [[maybe_unused]],
                                            const LogFlushPeriod* request,
                                            ::google::protobuf::Empty*) {
  log_v2::instance().set_flush_interval(request->period());
  return grpc::Status::OK;
}

/**
 * @brief get stats of the process (cpu, memory...)
 *
 * @param context
 * @param request
 * @param response
 * @return ::grpc::Status
 */
::grpc::Status broker_impl::GetProcessStats(
    ::grpc::ServerContext* context [[maybe_unused]],
    const ::google::protobuf::Empty* request [[maybe_unused]],
    ::com::centreon::common::pb_process_stat* response) {
  try {
    com::centreon::common::process_stat stat(getpid());
    stat.to_protobuff(*response);
  } catch (const boost::exception& e) {
    SPDLOG_LOGGER_ERROR(log_v2::instance().get(log_v2::CORE),
                        "fail to get process info: {}",
                        boost::diagnostic_information(e));

    return grpc::Status(grpc::StatusCode::INTERNAL,
                        boost::diagnostic_information(e));
  }
  return grpc::Status::OK;
}

grpc::Status broker_impl::Aes256Encrypt(grpc::ServerContext* context
                                        [[maybe_unused]],
                                        const AesMessage* request,
                                        GenericString* response) {
  std::string first_key = request->app_secret();
  std::string second_key = request->salt();

  try {
    aes256 access(first_key, second_key);
    std::string result = access.encrypt(request->content());
    response->set_str_arg(result);
    return grpc::Status::OK;
  } catch (const std::exception& e) {
    return grpc::Status(grpc::INVALID_ARGUMENT, grpc::string(e.what()));
  }
}

grpc::Status broker_impl::Aes256Decrypt(grpc::ServerContext* context
                                        [[maybe_unused]],
                                        const AesMessage* request,
                                        GenericString* response) {
  std::string first_key = request->app_secret();
  std::string second_key = request->salt();

  try {
    aes256 access(first_key, second_key);
    std::string result = access.decrypt(request->content());
    response->set_str_arg(result);
    return grpc::Status::OK;
  } catch (const std::exception& e) {
    return grpc::Status(grpc::INVALID_ARGUMENT, grpc::string(e.what()));
  }
}

grpc::Status broker_impl::GetPeers(grpc::ServerContext* context
                                   [[maybe_unused]],
                                   const ::google::protobuf::Empty* request
                                   [[maybe_unused]],
                                   PeerList* response) {
  for (auto& p : config::applier::state::instance().connected_peers()) {
    auto peer = response->add_peers();
    peer->set_id(p.poller_id);
    peer->set_poller_name(p.poller_name);
    peer->set_broker_name(p.broker_name);
    peer->mutable_connected_since()->set_seconds(p.connected_since);
    peer->set_type(p.peer_type);
  }
  return grpc::Status::OK;
}
