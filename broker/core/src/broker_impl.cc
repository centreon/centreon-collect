/*
 * Copyright 2020-2023 Centreon (https://www.centreon.com/)
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

#include "com/centreon/broker/config/applier/endpoint.hh"
#include "com/centreon/broker/config/applier/state.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/broker/multiplexing/publisher.hh"
#include "com/centreon/broker/stats/center.hh"
#include "com/centreon/broker/stats/helper.hh"
#include "com/centreon/broker/version.hh"

using namespace com::centreon::broker;
using namespace com::centreon::broker::version;

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
                                     __attribute__((unused)),
                                     const ::google::protobuf::Empty* request
                                     __attribute__((unused)),
                                     Version* response) {
  response->set_major(major);
  response->set_minor(minor);
  response->set_patch(patch);
  return grpc::Status::OK;
}

grpc::Status broker_impl::GetNumModules(grpc::ServerContext* context
                                        __attribute__((unused)),
                                        const ::google::protobuf::Empty*,
                                        GenericSize* response) {
  auto& mod_applier(config::applier::state::instance().get_modules());

  std::lock_guard<std::mutex> lock(mod_applier.module_mutex());
  response->set_size(std::distance(mod_applier.begin(), mod_applier.end()));

  return grpc::Status::OK;
}

grpc::Status broker_impl::GetNumEndpoint(grpc::ServerContext* context
                                         __attribute__((unused)),
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
                                          __attribute__((unused)),
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
                                           __attribute__((unused)),
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
    grpc::ServerContext* context __attribute__((unused)),
    const ::google::protobuf::Empty* request __attribute__((unused)),
    GenericString* response) {
  nlohmann::json object;
  stats::get_generic_stats(object);

  response->set_str_arg(object.dump());
  return grpc::Status::OK;
}

grpc::Status broker_impl::GetSqlManagerStats(
    grpc::ServerContext* context __attribute__((unused)),
    const ::google::protobuf::Empty* request __attribute__((unused)),
    SqlManagerStats* response) {
  stats::center::instance().get_sql_manager_stats(response);
  return grpc::Status::OK;
}

grpc::Status broker_impl::GetConflictManagerStats(
    grpc::ServerContext* context __attribute__((unused)),
    const ::google::protobuf::Empty* request __attribute__((unused)),
    ConflictManagerStats* response) {
  stats::center::instance().get_conflict_manager_stats(response);
  return grpc::Status::OK;
}

grpc::Status broker_impl::GetMuxerStats(grpc::ServerContext* context
                                        __attribute__((unused)),
                                        const GenericString* request,
                                        MuxerStats* response) {
  const std::string name = request->str_arg();
  bool status = stats::center::instance().muxer_stats(name, response);
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
                                           __attribute__((unused)),
                                           const IndexIds* request,
                                           ::google::protobuf::Empty* response
                                           __attribute__((unused))) {
  multiplexing::publisher pblshr;
  auto e{std::make_shared<bbdo::pb_rebuild_graphs>(*request)};
  pblshr.write(e);
  return grpc::Status::OK;
}

grpc::Status broker_impl::RemoveGraphs(grpc::ServerContext* context
                                       __attribute__((unused)),
                                       const ToRemove* request,
                                       ::google::protobuf::Empty* response
                                       __attribute__((unused))) {
  multiplexing::publisher pblshr;
  auto e{std::make_shared<bbdo::pb_remove_graphs>(*request)};
  pblshr.write(e);
  return grpc::Status::OK;
}

grpc::Status broker_impl::GetProcessingStats(
    grpc::ServerContext* context __attribute__((unused)),
    const ::google::protobuf::Empty* request __attribute__((unused)),
    ::ProcessingStats* response) {
  stats::center::instance().get_processing_stats(response);
  return grpc::Status::OK;
}

grpc::Status broker_impl::RemovePoller(grpc::ServerContext* context
                                       __attribute__((unused)),
                                       const GenericNameOrIndex* request,
                                       ::google::protobuf::Empty*) {
  log_v2::core()->info("Remove poller...");
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
  auto lvs = log_v2::instance()->levels();
  response->set_log_name(log_v2::instance()->log_name());
  response->set_log_file(log_v2::instance()->file_path());
  response->set_log_flush_period(
      log_v2::instance()->get_flush_interval().count());
  if (!name.empty()) {
    auto found = std::find_if(lvs.begin(), lvs.end(),
                              [&name](std::pair<std::string, std::string>& p) {
                                return p.first == name;
                              });
    if (found != lvs.end()) {
      map[name] = std::move(found->second);
      return grpc::Status::OK;
    } else {
      std::string msg{fmt::format("'{}' is not a logger in broker", name)};
      return grpc::Status(grpc::StatusCode::INVALID_ARGUMENT, msg);
    }
  } else {
    for (auto& p : lvs)
      map[p.first] = p.second;
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
    SPDLOG_LOGGER_ERROR(log_v2::core(), err_detail);
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
  bool done = false;
  spdlog::apply_all([&](const std::shared_ptr<spdlog::logger> logger) {
    if (!done) {
      std::shared_ptr<com::centreon::engine::log_v2_logger> logger_base =
          std::dynamic_pointer_cast<com::centreon::engine::log_v2_logger>(
              logger);
      if (logger_base) {
        logger_base->get_parent()->set_flush_interval(request->period());
        done = true;
      }
    }
  });
  return grpc::Status::OK;
}
