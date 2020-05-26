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

#include <sys/types.h>
#include <unistd.h>
#include <functional>
#include <future>

#include <google/protobuf/util/time_util.h>
#include "com/centreon/engine/anomalydetection.hh"
#include "com/centreon/engine/command_manager.hh"
#include "com/centreon/engine/contact.hh"
#include "com/centreon/engine/contactgroup.hh"
#include "com/centreon/engine/engine_impl.hh"
#include "com/centreon/engine/globals.hh"
#include "com/centreon/engine/host.hh"
#include "com/centreon/engine/hostdependency.hh"
#include "com/centreon/engine/hostgroup.hh"
#include "com/centreon/engine/logging.hh"
#include "com/centreon/engine/logging/logger.hh"
#include "com/centreon/engine/service.hh"
#include "com/centreon/engine/servicedependency.hh"
#include "com/centreon/engine/servicegroup.hh"
#include "com/centreon/engine/statistics.hh"
#include "engine-version.hh"

using namespace com::centreon::engine;
using namespace com::centreon::engine::logging;

/**
 * @brief Return the Engine's version.
 *
 * @param context gRPC context
 * @param  unused
 * @param response A Version object to fill
 *
 * @return Status::OK
 */
grpc::Status engine_impl::GetVersion(
    grpc::ServerContext* /*context*/,
    const ::google::protobuf::Empty* /*request*/,
    Version* response) {
  response->set_major(CENTREON_ENGINE_VERSION_MAJOR);
  response->set_minor(CENTREON_ENGINE_VERSION_MINOR);
  response->set_patch(CENTREON_ENGINE_VERSION_PATCH);
  return grpc::Status::OK;
}

grpc::Status engine_impl::GetStats(grpc::ServerContext* /*context*/,
                                   const ::google::protobuf::Empty* /*request*/,
                                   Stats* response) {
  response->mutable_status_file()->set_name(config->status_file());
  time_t now = time(nullptr);
  std::ifstream status_file;
  status_file.open(config->status_file());
  std::string line;
  time_t created;
  while (std::getline(status_file, line)) {
    size_t r = line.find("created=");
    if (r != std::string::npos) {
      created = std::stol(line.c_str() + r + strlen("created="), NULL, 10);
      break;
    }
  }
  com::centreon::engine::statistics& s =
      com::centreon::engine::statistics::instance();
  *response->mutable_status_file()->mutable_age() =
      google::protobuf::util::TimeUtil::SecondsToDuration(now - created);
  *response->mutable_program_status()->mutable_running_time() =
      google::protobuf::util::TimeUtil::SecondsToDuration(now - program_start);
  response->mutable_program_status()->set_pid(s.get_pid());

  buffer_stats stats;
  if (s.get_external_command_buffer_stats(stats)) {
    response->mutable_buffer()->set_used(stats.used);
    response->mutable_buffer()->set_high(stats.high);
    response->mutable_buffer()->set_total(stats.total);
  }
  return grpc::Status::OK;
}

grpc::Status engine_impl::GetHost(grpc::ServerContext* context, const HostIdentifier* request, EngineHost* response) {
  std::promise<EngineHost*> hostpromise;
  std::future<EngineHost*> f1 = hostpromise.get_future();
  EngineHost host;

  auto lambda = [&hostpromise, request, &host]() -> int32_t {
	std::shared_ptr<com::centreon::engine::host> selectedhost;
	
	switch (request->identifier_case()) {
	  case HostIdentifier::kHostName:
		selectedhost =  host::hosts.find(request->host_name())->second;
		break;
	  case HostIdentifier::kId:
		selectedhost =  host::hosts_by_id.find(request->id())->second;
		  break;
	  default:
	    return (1);
		break;
	}

    host.set_name(selectedhost->get_name());
    host.set_alias(selectedhost->get_alias());
    host.set_address(selectedhost->get_address());
	hostpromise.set_value(&host);

    return (0);
  };

  command_manager::instance().enqueue(lambda);

  *response = *(f1.get());
  //response->set_name(hosttmp->name());	
  //response->set_alias(hosttmp->alias());	
  //response->set_address(hosttmp->address());	

  return grpc::Status::OK;
}


grpc::Status engine_impl::GetHostsCount(
    grpc::ServerContext* context,
    const ::google::protobuf::Empty* request,
    GenericValue* response) {
  std::promise<int32_t> p;
  std::future<int32_t> f1 = p.get_future();

  auto lambda = [&p]() -> int32_t {
    p.set_value(host::hosts.size());
    return 0;
  };

  command_manager::instance().enqueue(lambda);

  int32_t val = f1.get();
  response->set_value(val);

  return grpc::Status::OK;
}

grpc::Status engine_impl::GetContactsCount(
    grpc::ServerContext* context,
    const ::google::protobuf::Empty* request,
    GenericValue* response) {
  std::promise<int32_t> p;
  std::future<int32_t> f1 = p.get_future();

  auto lambda = [&p]() -> int32_t {
    p.set_value(contact::contacts.size());
    return 0;
  };

  command_manager::instance().enqueue(lambda);

  int32_t val = f1.get();
  response->set_value(val);

  return grpc::Status::OK;
}

grpc::Status engine_impl::GetServicesCount(
    grpc::ServerContext* context,
    const ::google::protobuf::Empty* request,
    GenericValue* response) {
  std::promise<int32_t> p;
  std::future<int32_t> f1 = p.get_future();

  auto lambda = [&p]() -> int32_t {
    p.set_value(service::services.size());
    return 0;
  };

  command_manager::instance().enqueue(lambda);

  int32_t val = f1.get();
  response->set_value(val);
  return grpc::Status::OK;
}

grpc::Status engine_impl::GetServiceGroupsCount(
    grpc::ServerContext* context,
    const ::google::protobuf::Empty* request,
    GenericValue* response) {
  std::promise<int32_t> p;
  std::future<int32_t> f1 = p.get_future();

  auto lambda = [&p]() -> int32_t {
    p.set_value(servicegroup::servicegroups.size());
    return 0;
  };

  command_manager::instance().enqueue(lambda);

  int32_t val = f1.get();
  response->set_value(val);

  return grpc::Status::OK;
}

grpc::Status engine_impl::GetContactGroupsCount(
    grpc::ServerContext* context,
    const ::google::protobuf::Empty* request,
    GenericValue* response) {
  std::promise<int32_t> p;
  std::future<int32_t> f1 = p.get_future();

  auto lambda = [&p]() -> int32_t {
    p.set_value(contactgroup::contactgroups.size());
    return 0;
  };

  command_manager::instance().enqueue(lambda);

  int32_t val = f1.get();
  response->set_value(val);

  return grpc::Status::OK;
}

grpc::Status engine_impl::GetHostGroupsCount(
    grpc::ServerContext* context,
    const ::google::protobuf::Empty* request,
    GenericValue* response) {
  std::promise<int32_t> p;
  std::future<int32_t> f1 = p.get_future();

  auto lambda = [&p]() -> int32_t {
    p.set_value(hostgroup::hostgroups.size());
    return 0;
  };

  command_manager::instance().enqueue(lambda);

  int32_t val = f1.get();
  response->set_value(val);

  return grpc::Status::OK;
}

grpc::Status engine_impl::GetServiceDependenciesCount(
    grpc::ServerContext* context,
    const ::google::protobuf::Empty* request,
    GenericValue* response) {
  std::promise<int32_t> p;
  std::future<int32_t> f1 = p.get_future();

  auto lambda = [&p]() -> int32_t {
    p.set_value(servicedependency::servicedependencies.size());
    return 0;
  };

  command_manager::instance().enqueue(lambda);

  int32_t val = f1.get();
  response->set_value(val);

  return grpc::Status::OK;
}

grpc::Status engine_impl::GetHostDependenciesCount(
    grpc::ServerContext* context,
    const ::google::protobuf::Empty* request,
    GenericValue* response) {
  std::promise<int> p;
  std::future<int> f1 = p.get_future();

  auto lambda = [&p]() -> int {
    p.set_value(hostdependency::hostdependencies.size());
    return 0;
  };

  command_manager::instance().enqueue(lambda);

  int val = f1.get();
  response->set_value(val);

  return grpc::Status::OK;
}

grpc::Status engine_impl::ProcessServiceCheckResult(
    grpc::ServerContext* /*context*/,
    const Check* request,
    CommandSuccess* /*response*/) {
  std::string const& host_name = request->host_name();
  if (host_name.empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "host_name must not be empty");

  std::string const& svc_desc = request->svc_desc();
  if (svc_desc.empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "svc_desc must not be empty");

  auto fn = std::bind(&command_manager::process_passive_service_check,
                      &command_manager::instance(),
                      google::protobuf::util::TimeUtil::TimestampToSeconds(
                          request->check_time()),
                      host_name, svc_desc, request->code(), request->output());
  command_manager::instance().enqueue(std::move(fn));

  return grpc::Status::OK;
}

grpc::Status engine_impl::ProcessHostCheckResult(
    grpc::ServerContext* /*context*/,
    const Check* request,
    CommandSuccess* /*response*/) {
  std::string const& host_name = request->host_name();
  if (host_name.empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "host_name must not be empty");

  auto fn = std::bind(&command_manager::process_passive_host_check,
                      &command_manager::instance(),
                      google::protobuf::util::TimeUtil::TimestampToSeconds(
                          request->check_time()),
                      host_name, request->code(), request->output());
  command_manager::instance().enqueue(std::move(fn));

  return grpc::Status::OK;
}

/**
 * @brief When a new file arrives on the centreon server, this command is used
 * to notify engine to update its anomaly detection services with those new
 * thresholds. The update is done by the main loop thread because of concurrent
 * accesses.
 *
 * @param
 * @param request The Protobuf message containing the full name of the
 *                thresholds file.
 * @param
 *
 * @return A grpc::Status::OK if the file is well read, an error otherwise.
 */
grpc::Status engine_impl::NewThresholdsFile(grpc::ServerContext* /*context*/,
                                            const ThresholdsFile* request,
                                            CommandSuccess* /*response*/) {
  const std::string& filename = request->filename();
  auto fn = std::bind(&anomalydetection::update_thresholds, filename);
  command_manager::instance().enqueue(std::move(fn));
  return grpc::Status::OK;
}
