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

#include "com/centreon/engine/engine_impl.hh"

#include <google/protobuf/util/time_util.h>
#include <sys/types.h>
#include <unistd.h>

#include <functional>
#include <future>

#include "com/centreon/engine/anomalydetection.hh"
#include "com/centreon/engine/command_manager.hh"
#include "com/centreon/engine/comment.hh"
#include "com/centreon/engine/contact.hh"
#include "com/centreon/engine/contactgroup.hh"
#include "com/centreon/engine/downtimes/downtime.hh"
#include "com/centreon/engine/downtimes/downtime_finder.hh"
#include "com/centreon/engine/downtimes/downtime_manager.hh"
#include "com/centreon/engine/downtimes/service_downtime.hh"
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
using namespace com::centreon::engine::downtimes;

/** * @brief Return the Engine's version.
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

grpc::Status engine_impl::GetStats(grpc::ServerContext* context
                                   __attribute__((unused)),
                                   const GenericString* request
                                   __attribute__((unused)),
                                   Stats* response) {
  auto fn = std::packaged_task<int(void)>(
      std::bind(&command_manager::get_stats, &command_manager::instance(),
                request->str_arg(), response));
  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));
  int32_t res = result.get();
  if (res == 0)
    return grpc::Status::OK;
  else
    return grpc::Status(grpc::StatusCode::UNKNOWN, "Unknown error");
}

/**
 * @brief Return host informations.
 *
 * @param context gRPC context
 * @param request Host's identifier (it can be a hostname or a hostid)
 * @param response The filled fields
 *
 * @return Status::OK
 */
grpc::Status engine_impl::GetHost(grpc::ServerContext* context
                                  __attribute__((unused)),
                                  const HostIdentifier* request
                                  __attribute__((unused)),
                                  EngineHost* response) {
  auto fn =
      std::packaged_task<int(void)>([request, host = response]() -> int32_t {
        std::shared_ptr<com::centreon::engine::host> selectedhost;

        switch (request->identifier_case()) {
          case HostIdentifier::kName: {
            auto ithostname = host::hosts.find(request->name());
            if (ithostname != host::hosts.end())
              selectedhost = ithostname->second;
            else
              return 1;
          } break;
          case HostIdentifier::kId: {
            auto ithostid = host::hosts_by_id.find(request->id());
            if (ithostid != host::hosts_by_id.end())
              selectedhost = ithostid->second;
            else
              return 1;
          } break;
          default:
            return 1;
            break;
        }

        host->set_name(selectedhost->get_name());
        host->set_alias(selectedhost->get_alias());
        host->set_address(selectedhost->get_address());
        host->set_check_period(selectedhost->get_check_period());
        host->set_current_state(
            static_cast<EngineHost::State>(selectedhost->get_current_state()));
        host->set_id(selectedhost->get_host_id());
        return 0;
      });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));
  int32_t res = result.get();
  if (res == 0)
    return grpc::Status::OK;
  else
    return grpc::Status(grpc::INVALID_ARGUMENT,
                        grpc::string("hostname not found"));
}

/**
 * @brief Return contact informations.
 *
 * @param context gRPC context
 * @param request Contact's identifier
 * @param response The filled fields
 *
 * @return Status::OK
 */
grpc::Status engine_impl::GetContact(grpc::ServerContext* context
                                     __attribute__((unused)),
                                     const ContactIdentifier* request,
                                     EngineContact* response) {
  auto fn =
      std::packaged_task<int(void)>([request, contact = response]() -> int32_t {
        std::shared_ptr<com::centreon::engine::contact> selectedcontact;
        auto itcontactname = contact::contacts.find(request->name());
        if (itcontactname != contact::contacts.end())
          selectedcontact = itcontactname->second;
        else
          return 1;

        contact->set_name(selectedcontact->get_name());
        contact->set_alias(selectedcontact->get_alias());
        contact->set_email(selectedcontact->get_email());
        return 0;
      });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get() == 0)
    return grpc::Status::OK;
  else
    return grpc::Status(grpc::INVALID_ARGUMENT,
                        grpc::string("contact not found"));
}

/**
 * @brief Return service informations.
 *
 * @param context gRPC context
 * @param request Service's identifier (it can be a hostname & servicename or a
 * hostid & serviceid)
 * @param response The filled fields
 *
 * @return Status::OK
 */
grpc::Status engine_impl::GetService(grpc::ServerContext* context,
                                     const ServiceIdentifier* request,
                                     EngineService* response) {
  auto fn =
      std::packaged_task<int(void)>([request, service = response]() -> int32_t {
        std::shared_ptr<com::centreon::engine::service> selectedservice;

        switch (request->identifier_case()) {
          case ServiceIdentifier::kNames: {
            NameIdentifier names = request->names();
            auto itservicenames = service::services.find(
                std::make_pair(names.host_name(), names.service_name()));
            if (itservicenames != service::services.end())
              selectedservice = itservicenames->second;
            else
              return 1;
          } break;
          case ServiceIdentifier::kIds: {
            IdIdentifier ids = request->ids();
            auto itserviceids = service::services_by_id.find(
                std::make_pair(ids.host_id(), ids.service_id()));
            if (itserviceids != service::services_by_id.end())
              selectedservice = itserviceids->second;
            else
              return 1;
          } break;
          default:
            return 1;
            break;
        }

        service->set_host_id(selectedservice->get_host_id());
        service->set_service_id(selectedservice->get_service_id());
        service->set_host_name(selectedservice->get_hostname());
        service->set_description(selectedservice->get_description());
        service->set_check_period(selectedservice->get_check_period());
        service->set_current_state(static_cast<EngineService::State>(
            selectedservice->get_current_state()));
        return 0;
      });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  if (result.get() == 0)
    return grpc::Status::OK;
  else
    return grpc::Status(grpc::INVALID_ARGUMENT,
                        grpc::string("service not found"));
}

/**
 * @brief Return the total number of hosts.
 *
 * @param context gRPC context
 * @param unused
 * @param response Map size
 *
 * @return Status::OK
 */
grpc::Status engine_impl::GetHostsCount(grpc::ServerContext* context
                                        __attribute__((unused)),
                                        const ::google::protobuf::Empty* request
                                        __attribute__((unused)),
                                        GenericValue* response) {
  auto fn = std::packaged_task<int32_t(void)>(
      []() -> int32_t { return host::hosts.size(); });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(result.get());

  return grpc::Status::OK;
}

/**
 * @brief Return the total number of contacts.
 *
 * @param context gRPC context
 * @param unused
 * @param response Map size
 *
 * @return Status::OK
 */
grpc::Status engine_impl::GetContactsCount(
    grpc::ServerContext* context __attribute__((unused)),
    const ::google::protobuf::Empty* request __attribute__((unused)),
    GenericValue* response) {
  auto fn = std::packaged_task<int32_t(void)>(
      []() -> int32_t { return contact::contacts.size(); });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(result.get());

  return grpc::Status::OK;
}

/**
 * @brief Return the total number of services.
 *
 * @param context gRPC context
 * @param unused
 * @param response Map size
 *
 * @return Status::OK
 */
grpc::Status engine_impl::GetServicesCount(
    grpc::ServerContext* context __attribute__((unused)),
    const ::google::protobuf::Empty* request __attribute__((unused)),
    GenericValue* response) {
  auto fn = std::packaged_task<int32_t(void)>(
      []() -> int32_t { return service::services.size(); });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(result.get());
  return grpc::Status::OK;
}

/**
 * @brief Return the total number of service groups.
 *
 * @param context gRPC context
 * @param unused
 * @param response Map size
 *
 * @return Status::OK
 */
grpc::Status engine_impl::GetServiceGroupsCount(
    grpc::ServerContext* context __attribute__((unused)),
    const ::google::protobuf::Empty* request __attribute__((unused)),
    GenericValue* response) {
  auto fn = std::packaged_task<int32_t(void)>(
      []() -> int32_t { return servicegroup::servicegroups.size(); });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(result.get());
  return grpc::Status::OK;
}

/**
 * @brief Return the total number of contact groups.
 *
 * @param context gRPC context
 * @param unused
 * @param response Map size
 *
 * @return Status::OK
 */
grpc::Status engine_impl::GetContactGroupsCount(
    grpc::ServerContext* context __attribute__((unused)),
    const ::google::protobuf::Empty* request __attribute__((unused)),
    GenericValue* response) {
  auto fn = std::packaged_task<int32_t(void)>(
      []() -> int32_t { return contactgroup::contactgroups.size(); });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(result.get());
  return grpc::Status::OK;
}

/**
 * @brief Return the total number of host groups.
 *
 * @param context gRPC context
 * @param unused
 * @param response Map size
 *
 * @return Status::OK
 */
grpc::Status engine_impl::GetHostGroupsCount(
    grpc::ServerContext* context __attribute__((unused)),
    const ::google::protobuf::Empty* request __attribute__((unused)),
    GenericValue* response) {
  auto fn = std::packaged_task<int32_t(void)>(
      []() -> int32_t { return hostgroup::hostgroups.size(); });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(result.get());
  return grpc::Status::OK;
}

/**
 * @brief Return the total number of service dependencies.
 *
 * @param context gRPC context
 * @param unused
 * @param response Map size
 *
 * @return Status::OK
 */
grpc::Status engine_impl::GetServiceDependenciesCount(
    grpc::ServerContext* context __attribute__((unused)),
    const ::google::protobuf::Empty* request __attribute__((unused)),
    GenericValue* response) {
  auto fn = std::packaged_task<int32_t(void)>([]() -> int32_t {
    return servicedependency::servicedependencies.size();
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(result.get());
  return grpc::Status::OK;
}

/**
 * @brief Return the total number of host dependencies.
 *
 * @param context gRPC context
 * @param unused
 * @param response Map size
 *
 * @return Status::OK
 */
grpc::Status engine_impl::GetHostDependenciesCount(
    grpc::ServerContext* context __attribute__((unused)),
    const ::google::protobuf::Empty* request __attribute__((unused)),
    GenericValue* response) {
  auto fn = std::packaged_task<int32_t(void)>(
      []() -> int32_t { return hostdependency::hostdependencies.size(); });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(result.get());
  return grpc::Status::OK;
}

grpc::Status engine_impl::AddHostComment(grpc::ServerContext* context
                                         __attribute__((unused)),
                                         const EngineComment* request,
                                         CommandSuccess* response) {
  auto fn = std::packaged_task<int32_t(void)>([request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    int32_t persistent;

    auto it = host::hosts.find(request->host_name());
    if (it != host::hosts.end())
      temp_host = it->second;
    if (temp_host == nullptr)
      return 1;
    if (request->persistent() > 1)
      persistent = 1;
    else if (request->persistent() < 0)
      persistent = 0;
    /* add the comment */
    auto cmt = std::make_shared<comment>(
        comment::host, comment::user, temp_host->get_host_id(), 0,
        request->entry_time(), request->user(), request->comment_data(),
        persistent, comment::external, false, (time_t)0);
    comment::comments.insert({cmt->get_comment_id(), cmt});
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(!result.get());
  return grpc::Status::OK;
}

grpc::Status engine_impl::AddServiceComment(grpc::ServerContext* context
                                            __attribute__((unused)),
                                            const EngineComment* request,
                                            CommandSuccess* response) {
  auto fn = std::packaged_task<int32_t(void)>([request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    std::shared_ptr<engine::service> temp_service;
    int32_t persistent;

    auto it =
        service::services.find({request->host_name(), request->svc_desc()});
    if (it != service::services.end())
      temp_service = it->second;
    if (temp_service == nullptr)
      return 1;
    auto it2 = host::hosts.find(request->host_name());
    if (it2 != host::hosts.end())
      temp_host = it2->second;
    if (temp_host == nullptr)
      return 1;
    if (request->persistent() > 1)
      persistent = 1;
    else if (request->persistent() < 0)
      persistent = 0;
    /* add the comment */
    auto cmt = std::make_shared<comment>(
        comment::service, comment::user, temp_host->get_host_id(),
        temp_service->get_service_id(), request->entry_time(), request->user(),
        request->comment_data(), persistent, comment::external, false,
        (time_t)0);
    comment::comments.insert({cmt->get_comment_id(), cmt});
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(!result.get());
  return grpc::Status::OK;
}

grpc::Status engine_impl::DeleteComment(grpc::ServerContext* context
                                        __attribute__((unused)),
                                        const GenericValue* request,
                                        CommandSuccess* response) {
  uint32_t comment_id = request->value();
  if (comment_id == 0)
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "comment_id must not be set to 0");

  auto fn = std::packaged_task<int32_t(void)>([&comment_id]() -> int32_t {
    comment::delete_comment(comment_id);
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(!result.get());
  return grpc::Status::OK;
}

/**
 * @brief Remove all comments from a host.
 *
 * @param context gRPC context
 * @param request Host's identifier (it can be a hostname or a hostid)
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DeleteAllHostComments(grpc::ServerContext* context
                                                __attribute__((unused)),
                                                const HostIdentifier* request,
                                                CommandSuccess* response) {
  auto fn = std::packaged_task<int32_t(void)>([request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;
    switch (request->identifier_case()) {
      case HostIdentifier::kName: {
        auto it = host::hosts.find(request->name());
        if (it != host::hosts.end())
          temp_host = it->second;
        if (temp_host == nullptr)
          return 1;
      } break;
      case HostIdentifier::kId: {
        auto it = host::hosts_by_id.find(request->id());
        if (it != host::hosts_by_id.end())
          temp_host = it->second;
        if (temp_host == nullptr)
          return 1;
      } break;
      default:
        return 1;
        break;
    }
    comment::delete_host_comments(temp_host->get_host_id());
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(!result.get());
  return grpc::Status::OK;
}

/**
 * @brief Remove all comments from a service.
 *
 * @param context gRPC context
 * @param request Service's identifier (it can be a hostname & servicename or a
 * hostid & serviceid)
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DeleteAllServiceComments(
    grpc::ServerContext* context __attribute__((unused)),
    const ServiceIdentifier* request,
    CommandSuccess* response) {
  auto fn = std::packaged_task<int32_t(void)>([request]() -> int32_t {
    std::shared_ptr<engine::service> temp_service;

    switch (request->identifier_case()) {
      case ServiceIdentifier::kNames: {
        NameIdentifier names = request->names();
        auto it =
            service::services.find({names.host_name(), names.service_name()});
        if (it != service::services.end())
          temp_service = it->second;
        if (temp_service == nullptr)
          return 1;
      } break;
      case ServiceIdentifier::kIds: {
        IdIdentifier ids = request->ids();
        auto it =
            service::services_by_id.find({ids.host_id(), ids.service_id()});
        if (it != service::services_by_id.end())
          temp_service = it->second;
        if (temp_service == nullptr)
          return 1;
      } break;
      default:
        return 1;
        break;
    }
    comment::delete_service_comments(temp_service->get_host_id(),
                                     temp_service->get_service_id());
    return 0;
  });
  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(!result.get());
  return grpc::Status::OK;
}

grpc::Status engine_impl::RemoveHostAcknowledgement(
    grpc::ServerContext* context __attribute__((unused)),
    const HostIdentifier* request,
    CommandSuccess* response) {
  auto fn = std::packaged_task<int32_t(void)>([request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;

    switch (request->identifier_case()) {
      case HostIdentifier::kName: {
        auto it = host::hosts.find(request->name());
        if (it != host::hosts.end())
          temp_host = it->second;
        if (temp_host == nullptr)
          return 1;
      } break;
      case HostIdentifier::kId: {
        auto it = host::hosts_by_id.find(request->id());
        if (it != host::hosts_by_id.end())
          temp_host = it->second;
        if (temp_host == nullptr)
          return 1;
      } break;
      default:
        return 1;
        break;
    }

    /* set the acknowledgement flag */
    temp_host->set_problem_has_been_acknowledged(false);
    /* update the status log with the host info */
    temp_host->update_status(false);
    /* remove any non-persistant comments associated with the ack */
    comment::delete_host_acknowledgement_comments(temp_host.get());
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(!result.get());
  return grpc::Status::OK;
}

grpc::Status engine_impl::RemoveServiceAcknowledgement(
    grpc::ServerContext* context __attribute__((unused)),
    const ServiceIdentifier* request,
    CommandSuccess* response) {
  auto fn = std::packaged_task<int32_t(void)>([request]() -> int32_t {
    std::shared_ptr<engine::service> temp_service;

    switch (request->identifier_case()) {
      case ServiceIdentifier::kNames: {
        NameIdentifier names = request->names();
        auto it =
            service::services.find({names.host_name(), names.service_name()});
        if (it != service::services.end())
          temp_service = it->second;
        if (temp_service == nullptr)
          return 1;
      } break;
      case ServiceIdentifier::kIds: {
        IdIdentifier ids = request->ids();
        auto it =
            service::services_by_id.find({ids.host_id(), ids.service_id()});
        if (it != service::services_by_id.end())
          temp_service = it->second;
        if (temp_service == nullptr)
          return 1;
      } break;
      default:
        return 1;
        break;
    }

    /* set the acknowledgement flag */
    temp_service->set_problem_has_been_acknowledged(false);
    /* update the status log with the service info */
    temp_service->update_status(false);
    /* remove any non-persistant comments associated with the ack */
    comment::delete_service_acknowledgement_comments(temp_service.get());
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(!result.get());
  return grpc::Status::OK;
}

grpc::Status engine_impl::DeleteHostDowntime(grpc::ServerContext* context
                                             __attribute__((unused)),
                                             const GenericValue* request,
                                             CommandSuccess* response) {
  uint32_t downtime_id = request->value();
  auto fn = std::packaged_task<int32_t(void)>([&downtime_id]() -> int32_t {
    /* deletes scheduled host downtime */
    if (downtime_manager::instance().unschedule_downtime(
            downtime::host_downtime, downtime_id) == ERROR)
      return 1;
    else
      return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(!result.get());
  return grpc::Status::OK;
}

grpc::Status engine_impl::DeleteServiceDowntime(grpc::ServerContext* context
                                                __attribute__((unused)),
                                                const GenericValue* request,
                                                CommandSuccess* response) {
  uint32_t downtime_id = request->value();
  auto fn = std::packaged_task<int32_t(void)>([&downtime_id]() -> int32_t {
    /* deletes scheduled service downtime */
    if (downtime_manager::instance().unschedule_downtime(
            downtime::service_downtime, downtime_id) == ERROR)
      return 1;
    else
      return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(!result.get());
  return grpc::Status::OK;
}

/**
 * @brief Delete scheduled host  downtime, according to some criterias.
 * If a criteria is not defined then it doesnt matter in the search
 * for downtime
 *
 * @param context gRPC context
 * @param request DowntimeCriterias (it can be a hostname, a start_time,
 * an end_time, a fixed, a triggered_by, a duration, an author
 * or a comment)
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DeleteHostDowntimeFull(
    grpc::ServerContext* context __attribute__((unused)),
    const DowntimeCriterias* request,
    CommandSuccess* response) {
  auto fn = std::packaged_task<int32_t(void)>([request]() -> int32_t {
    downtime::type downtime_type = downtime::host_downtime;
    std::list<std::shared_ptr<com::centreon::engine::downtimes::downtime>>
        dtlist;
    for (auto it = downtimes::downtime_manager::instance()
                       .get_scheduled_downtimes()
                       .begin(),
              end = downtimes::downtime_manager::instance()
                        .get_scheduled_downtimes()
                        .end();
         it != end; ++it) {
      auto dt = it->second;

      if (!(request->host_name().empty()) &&
          dt->get_hostname() != request->host_name())
        continue;
      if (request->has_start() &&
          dt->get_start_time() != request->start().value())
        continue;
      if (request->has_end() && dt->get_end_time() != request->end().value())
        continue;
      if (request->has_fixed() && dt->is_fixed() != request->fixed().value())
        continue;
      if (request->has_triggered_by() &&
          dt->get_triggered_by() != request->triggered_by().value())
        continue;
      if (request->has_duration() &&
          dt->get_duration() != request->duration().value())
        continue;
      if (!(request->author().empty()) && dt->get_author() != request->author())
        continue;
      if (!(request->comment_data().empty()) &&
          dt->get_comment() != request->comment_data())
        continue;
      dtlist.push_back(dt);
    }

    for (auto& d : dtlist)
      downtime_manager::instance().unschedule_downtime(downtime_type,
                                                       d->get_downtime_id());

    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(!result.get());
  return grpc::Status::OK;
}

/**
 * @brief Delete scheduled service downtime, according to some criterias.
 * If a criteria is not defined then it doesnt matter in the search
 * for downtime
 *
 * @param context gRPC context
 * @param request DowntimeCriterias (it can be a hostname, a service description
 * a start_time, an end_time, a fixed, a triggered_by, a duration, an author
 * or a comment)
 * @param response Command answer
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DeleteServiceDowntimeFull(
    grpc::ServerContext* context __attribute__((unused)),
    const DowntimeCriterias* request,
    CommandSuccess* response) {
  auto fn = std::packaged_task<int32_t(void)>([request]() -> int32_t {
    downtime::type downtime_type = downtime::service_downtime;
    std::list<service_downtime*> dtlist;
    /* iterate through all current downtime(s) */
    for (auto it = downtimes::downtime_manager::instance()
                       .get_scheduled_downtimes()
                       .begin(),
              end = downtimes::downtime_manager::instance()
                        .get_scheduled_downtimes()
                        .end();
         it != end; ++it) {
      service_downtime* dt = static_cast<service_downtime*>(it->second.get());
      /* we are checking if request criteria match with the downtime criteria */
      if (!(request->host_name().empty()) &&
          (dt->get_hostname() != request->host_name()))
        continue;
      if (!(request->service_desc().empty()) &&
          (dt->get_service_description() != request->service_desc()))
        continue;
      if (request->has_start() &&
          dt->get_start_time() != request->start().value())
        continue;
      if (request->has_end() && dt->get_end_time() != request->end().value())
        continue;
      if (request->has_fixed() && dt->is_fixed() != request->fixed().value())
        continue;
      if (request->has_triggered_by() &&
          dt->get_triggered_by() != request->triggered_by().value())
        continue;
      if (request->has_duration() &&
          dt->get_duration() != request->duration().value())
        continue;
      if (!(request->author().empty()) && dt->get_author() != request->author())
        continue;
      if (!(request->comment_data().empty()) &&
          dt->get_comment() != request->comment_data())
        continue;
      /* if all criterias match then we found a downtime to delete */
      dtlist.push_back(dt);
    }

    /* deleting downtime(s) */
    for (auto& d : dtlist)
      downtime_manager::instance().unschedule_downtime(downtime_type,
                                                       d->get_downtime_id());
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(!result.get());
  return grpc::Status::OK;
}

/**
 * @brief Deletes scheduled host and service downtime based on hostname and
 * optionnaly other filter arguments.
 *
 * @param context gRPC context
 * @param request DowntimeHostIdentifier (it's a hostname and optionally other
 * filter arguments like service description, start time and downtime's 
 * comment 
 * @param Command response 
 *
 * @return Status::OK
 */
grpc::Status engine_impl::DeleteDowntimeByHostName(
    grpc::ServerContext* context __attribute__((unused)),
    const DowntimeHostIdentifier* request,
    CommandSuccess* response) {

  /*hostname must be defined to delete the downtime but not others arguments*/
  std::string const& host_name = request->host_name();
  if (host_name.empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "host_name must not be empty");

  auto fn = std::packaged_task<int32_t(void)>([&host_name,
                                               request]() -> int32_t {
    time_t start_time;
    std::string service_desc;
    std::string comment_data;
    if (!(request->service_desc().empty()))
      service_desc = request->service_desc();
    if (!(request->comment_data().empty()))
      comment_data = request->comment_data();
    if (!(request->has_start()))
      start_time = 0;
    else
      start_time = request->start().value();

    uint32_t deleted =
        downtime_manager::instance()
            .delete_downtime_by_hostname_service_description_start_time_comment(
                host_name, service_desc, start_time,
                comment_data);
    if (deleted == 0)
      return 1;
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(!result.get());
  return grpc::Status::OK;
}

grpc::Status engine_impl::DelayHostNotification(
    grpc::ServerContext* context __attribute__((unused)),
    const HostDelayIdentifier* request,
    CommandSuccess* response) {
  auto fn = std::packaged_task<int32_t(void)>([request]() -> int32_t {
    std::shared_ptr<engine::host> temp_host;

    switch (request->identifier_case()) {
      case HostIdentifier::kName: {
        auto it = host::hosts.find(request->name());
        if (it != host::hosts.end())
          temp_host = it->second;
        if (temp_host == nullptr)
          return 1;
      } break;
      case HostIdentifier::kId: {
        auto it = host::hosts_by_id.find(request->id());
        if (it != host::hosts_by_id.end())
          temp_host = it->second;
        if (temp_host == nullptr)
          return 1;
      } break;
      default:
        return 1;
        break;
    }

    temp_host->set_next_notification(request->delay_time());
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(!result.get());
  return grpc::Status::OK;
}

grpc::Status engine_impl::DelayServiceNotification(
    grpc::ServerContext* context __attribute__((unused)),
    const ServiceDelayIdentifier* request,
    CommandSuccess* response) {
  auto fn = std::packaged_task<int32_t(void)>([request]() -> int32_t {
    std::shared_ptr<engine::service> temp_service;

    switch (request->identifier_case()) {
      case ServiceIdentifier::kNames: {
        NameIdentifier names = request->names();
        auto it =
            service::services.find({names.host_name(), names.service_name()});
        if (it != service::services.end())
          temp_service = it->second;
        if (temp_service == nullptr)
          return 1;
      } break;
      case ServiceIdentifier::kIds: {
        IdIdentifier ids = request->ids();
        auto it =
            service::services_by_id.find({ids.host_id(), ids.service_id()});
        if (it != service::services_by_id.end())
          temp_service = it->second;
        if (temp_service == nullptr)
          return 1;
      } break;
      default:
        return 1;
        break;
    }

    temp_service->set_next_notification(request->delay_time());
    return 0;
  });

  std::future<int32_t> result = fn.get_future();
  command_manager::instance().enqueue(std::move(fn));

  response->set_value(!result.get());
  return grpc::Status::OK;
}

grpc::Status engine_impl::ProcessServiceCheckResult(grpc::ServerContext* context
                                                    __attribute__((unused)),
                                                    const Check* request,
                                                    CommandSuccess* response
                                                    __attribute__((unused))) {
  std::string const& host_name = request->host_name();
  if (host_name.empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "host_name must not be empty");

  std::string const& svc_desc = request->svc_desc();
  if (svc_desc.empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "svc_desc must not be empty");

  auto fn = std::packaged_task<int(void)>(
      std::bind(&command_manager::process_passive_service_check,
                &command_manager::instance(),
                google::protobuf::util::TimeUtil::TimestampToSeconds(
                    request->check_time()),
                host_name, svc_desc, request->code(), request->output()));
  command_manager::instance().enqueue(std::move(fn));

  return grpc::Status::OK;
}

grpc::Status engine_impl::ProcessHostCheckResult(grpc::ServerContext* context
                                                 __attribute__((unused)),
                                                 const Check* request,
                                                 CommandSuccess* response
                                                 __attribute__((unused))) {
  std::string const& host_name = request->host_name();
  if (host_name.empty())
    return grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT,
                        "host_name must not be empty");

  auto fn = std::packaged_task<int(void)>(
      std::bind(&command_manager::process_passive_host_check,
                &command_manager::instance(),
                google::protobuf::util::TimeUtil::TimestampToSeconds(
                    request->check_time()),
                host_name, request->code(), request->output()));
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
grpc::Status engine_impl::NewThresholdsFile(grpc::ServerContext* context
                                            __attribute__((unused)),
                                            const ThresholdsFile* request,
                                            CommandSuccess* response
                                            __attribute__((unused))) {
  const std::string& filename = request->filename();
  auto fn = std::packaged_task<int(void)>(
      std::bind(&anomalydetection::update_thresholds, filename));
  command_manager::instance().enqueue(std::move(fn));
  return grpc::Status::OK;
}
