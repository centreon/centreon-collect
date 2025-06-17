#ifndef CCE_ENGINERPC_ENGINE_IMPL_HH
#define CCE_ENGINERPC_ENGINE_IMPL_HH

#include "engine/enginerpc/engine.grpc.pb.h"

namespace com::centreon::engine {
class engine_impl final : public Engine::Service {
  grpc::Status GetVersion(grpc::ServerContext* context,
                          const ::google::protobuf::Empty* /*request*/,
                          Version* response) override;
  grpc::Status GetStats(grpc::ServerContext* context,
                        const GenericString* request,
                        Stats* response) override;
  grpc::Status GetProcessStats(
      grpc::ServerContext* context,
      const google::protobuf::Empty* request,
      com::centreon::common::pb_process_stat* response) override;
  grpc::Status ProcessServiceCheckResult(grpc::ServerContext* context,
                                         const Check* request,
                                         CommandSuccess* response) override;
  grpc::Status ProcessHostCheckResult(grpc::ServerContext* context,
                                      const Check* request,
                                      CommandSuccess* response) override;
  grpc::Status NewThresholdsFile(grpc::ServerContext* context,
                                 const ThresholdsFile* request,
                                 CommandSuccess* response) override;
  grpc::Status GetHostsCount(grpc::ServerContext* context,
                             const ::google::protobuf::Empty*,
                             GenericValue*) override;
  grpc::Status GetContactsCount(grpc::ServerContext* context,
                                const ::google::protobuf::Empty*,
                                GenericValue*) override;
  grpc::Status GetServicesCount(grpc::ServerContext* context,
                                const ::google::protobuf::Empty*,
                                GenericValue*) override;
  grpc::Status GetServiceGroupsCount(grpc::ServerContext* context,
                                     const ::google::protobuf::Empty*,
                                     GenericValue*) override;
  grpc::Status GetContactGroupsCount(grpc::ServerContext* context,
                                     const ::google::protobuf::Empty*,
                                     GenericValue*) override;
  grpc::Status GetHostGroupsCount(grpc::ServerContext* context,
                                  const ::google::protobuf::Empty*,
                                  GenericValue*) override;
  grpc::Status GetServiceDependenciesCount(grpc::ServerContext* context,
                                           const ::google::protobuf::Empty*,
                                           GenericValue*) override;
  grpc::Status GetHostDependenciesCount(grpc::ServerContext* context,
                                        const ::google::protobuf::Empty*,
                                        GenericValue*) override;
  grpc::Status GetHost(grpc::ServerContext* context,
                       const HostIdentifier* request,
                       EngineHost* response) override;
  grpc::Status GetContact(grpc::ServerContext* context,
                          const ContactIdentifier* request,
                          EngineContact* response) override;
  grpc::Status GetService(grpc::ServerContext* context,
                          const ServiceIdentifier* request,
                          EngineService* response) override;
  grpc::Status AddHostComment(grpc::ServerContext* context,
                              const EngineComment* request,
                              CommandSuccess* response) override;
  grpc::Status AddServiceComment(grpc::ServerContext* context,
                                 const EngineComment* request,
                                 CommandSuccess* response) override;
  grpc::Status DeleteComment(grpc::ServerContext* context,
                             const GenericValue* request,
                             CommandSuccess* response) override;
  grpc::Status DeleteAllServiceComments(grpc::ServerContext* context,
                                        const ServiceIdentifier* request,
                                        CommandSuccess* response) override;
  grpc::Status DeleteAllHostComments(grpc::ServerContext* context,
                                     const HostIdentifier* request,
                                     CommandSuccess* response) override;
  grpc::Status RemoveHostAcknowledgement(grpc::ServerContext* context,
                                         const HostIdentifier* request,
                                         CommandSuccess* response) override;
  grpc::Status RemoveServiceAcknowledgement(grpc::ServerContext* context,
                                            const ServiceIdentifier* request,
                                            CommandSuccess* response) override;
  grpc::Status AcknowledgementHostProblem(grpc::ServerContext* context,
                                          const EngineAcknowledgement* request,
                                          CommandSuccess* response) override;
  grpc::Status AcknowledgementServiceProblem(
      grpc::ServerContext* context,
      const EngineAcknowledgement* request,
      CommandSuccess* response) override;
  grpc::Status ScheduleHostDowntime(grpc::ServerContext* context,
                                    const ScheduleDowntimeIdentifier* request,
                                    CommandSuccess* response) override;
  grpc::Status ScheduleServiceDowntime(
      grpc::ServerContext* context,
      const ScheduleDowntimeIdentifier* request,
      CommandSuccess* response) override;
  grpc::Status ScheduleHostServicesDowntime(
      grpc::ServerContext* context,
      const ScheduleDowntimeIdentifier* request,
      CommandSuccess* response) override;
  grpc::Status ScheduleHostGroupHostsDowntime(
      grpc::ServerContext* context,
      const ScheduleDowntimeIdentifier* request,
      CommandSuccess* response) override;
  grpc::Status ScheduleHostGroupServicesDowntime(
      grpc::ServerContext* context,
      const ScheduleDowntimeIdentifier* request,
      CommandSuccess* response) override;
  grpc::Status ScheduleServiceGroupHostsDowntime(
      grpc::ServerContext* context,
      const ScheduleDowntimeIdentifier* request,
      CommandSuccess* response) override;
  grpc::Status ScheduleServiceGroupServicesDowntime(
      grpc::ServerContext* context,
      const ScheduleDowntimeIdentifier* request,
      CommandSuccess* response) override;
  grpc::Status ScheduleAndPropagateHostDowntime(
      grpc::ServerContext* context,
      const ScheduleDowntimeIdentifier* request,
      CommandSuccess* response) override;
  grpc::Status ScheduleAndPropagateTriggeredHostDowntime(
      grpc::ServerContext* context,
      const ScheduleDowntimeIdentifier* request,
      CommandSuccess* response) override;
  grpc::Status DeleteDowntime(grpc::ServerContext* context,
                              const GenericValue* request,
                              CommandSuccess* response) override;
  grpc::Status DeleteHostDowntimeFull(grpc::ServerContext* context,
                                      const DowntimeCriterias* request,
                                      CommandSuccess* response) override;
  grpc::Status DeleteServiceDowntimeFull(grpc::ServerContext* context,
                                         const DowntimeCriterias* request,
                                         CommandSuccess* response) override;
  grpc::Status DeleteDowntimeByHostName(grpc::ServerContext* context,
                                        const DowntimeHostIdentifier* request,
                                        CommandSuccess* response) override;
  grpc::Status DeleteDowntimeByHostGroupName(
      grpc::ServerContext* context,
      const DowntimeHostGroupIdentifier* request,
      CommandSuccess* response) override;
  grpc::Status DeleteDowntimeByStartTimeComment(
      grpc::ServerContext* context,
      const DowntimeStartTimeIdentifier* request,
      CommandSuccess* response) override;
  grpc::Status ScheduleHostCheck(grpc::ServerContext* context,
                                 const HostCheckIdentifier* request,
                                 CommandSuccess* response) override;
  grpc::Status ScheduleHostServiceCheck(grpc::ServerContext* context,
                                        const HostCheckIdentifier* request,
                                        CommandSuccess* response) override;
  grpc::Status ScheduleServiceCheck(grpc::ServerContext* context,
                                    const ServiceCheckIdentifier* request,
                                    CommandSuccess* response) override;
  grpc::Status SignalProcess(grpc::ServerContext* context,
                             const EngineSignalProcess* request,
                             CommandSuccess* response) override;
  grpc::Status DelayHostNotification(grpc::ServerContext* context,
                                     const HostDelayIdentifier* request,
                                     CommandSuccess* response) override;
  grpc::Status DelayServiceNotification(grpc::ServerContext* context,
                                        const ServiceDelayIdentifier* request,
                                        CommandSuccess* response) override;
  grpc::Status ChangeHostObjectIntVar(grpc::ServerContext* context,
                                      const ChangeObjectInt* request,
                                      CommandSuccess* response) override;
  grpc::Status ChangeServiceObjectIntVar(grpc::ServerContext* context,
                                         const ChangeObjectInt* request,
                                         CommandSuccess* response) override;
  grpc::Status ChangeContactObjectIntVar(grpc::ServerContext* context,
                                         const ChangeContactObjectInt* request,
                                         CommandSuccess* response) override;
  grpc::Status ChangeHostObjectCharVar(grpc::ServerContext* context,
                                       const ChangeObjectChar* request,
                                       CommandSuccess* response) override;
  grpc::Status ChangeServiceObjectCharVar(grpc::ServerContext* context,
                                          const ChangeObjectChar* request,
                                          CommandSuccess* response) override;
  grpc::Status ChangeContactObjectCharVar(
      grpc::ServerContext* context,
      const ChangeContactObjectChar* request,
      CommandSuccess* response) override;
  grpc::Status ChangeHostObjectCustomVar(grpc::ServerContext* context
                                         __attribute__((unused)),
                                         const ChangeObjectCustomVar* request,
                                         CommandSuccess* response) override;
  grpc::Status ChangeServiceObjectCustomVar(
      grpc::ServerContext* context __attribute__((unused)),
      const ChangeObjectCustomVar* request,
      CommandSuccess* response) override;
  grpc::Status ChangeContactObjectCustomVar(
      grpc::ServerContext* context __attribute__((unused)),
      const ChangeObjectCustomVar* request,
      CommandSuccess* response) override;
  grpc::Status ShutdownProgram(grpc::ServerContext* context,
                               const ::google::protobuf::Empty*,
                               ::google::protobuf::Empty*) override;
  ::grpc::Status EnableHostAndChildNotifications(
      ::grpc::ServerContext* context,
      const ::com::centreon::engine::HostIdentifier* request,
      ::com::centreon::engine::CommandSuccess* response) override;
  ::grpc::Status DisableHostAndChildNotifications(
      ::grpc::ServerContext* context,
      const ::com::centreon::engine::HostIdentifier* request,
      ::com::centreon::engine::CommandSuccess* response) override;

  ::grpc::Status DisableHostNotifications(
      ::grpc::ServerContext* context,
      const ::com::centreon::engine::HostIdentifier* request,
      ::com::centreon::engine::CommandSuccess* response) override;

  ::grpc::Status EnableHostNotifications(
      ::grpc::ServerContext* context,
      const ::com::centreon::engine::HostIdentifier* request,
      ::com::centreon::engine::CommandSuccess* response) override;

  ::grpc::Status DisableNotifications(
      ::grpc::ServerContext* context,
      const ::google::protobuf::Empty*,
      ::com::centreon::engine::CommandSuccess* response) override;

  ::grpc::Status EnableNotifications(
      ::grpc::ServerContext* context,
      const ::google::protobuf::Empty*,
      ::com::centreon::engine::CommandSuccess* response) override;

  ::grpc::Status DisableServiceNotifications(
      ::grpc::ServerContext* context,
      const ::com::centreon::engine::ServiceIdentifier* service,
      ::com::centreon::engine::CommandSuccess* response) override;

  ::grpc::Status EnableServiceNotifications(
      ::grpc::ServerContext* context,
      const ::com::centreon::engine::ServiceIdentifier* service,
      ::com::centreon::engine::CommandSuccess* response) override;

  ::grpc::Status ChangeAnomalyDetectionSensitivity(
      ::grpc::ServerContext* context,
      const ::com::centreon::engine::ChangeServiceNumber* serv_and_value,
      ::com::centreon::engine::CommandSuccess* response) override;

  static std::pair<std::shared_ptr<com::centreon::engine::host>,
                   std::string /*error*/>
  get_host(const ::com::centreon::engine::HostIdentifier& host_info);

  static std::pair<std::shared_ptr<com::centreon::engine::service>,
                   std::string /*error*/>
  get_serv(const ::com::centreon::engine::ServiceIdentifier& serv_info);

  ::grpc::Status GetLogInfo(
      ::grpc::ServerContext* context,
      const ::google::protobuf::Empty* request,
      ::com::centreon::engine::LogInfo* response) override;

  grpc::Status SetLogLevel(grpc::ServerContext* context [[maybe_unused]],
                           const LogLevel* request,
                           ::google::protobuf::Empty*) override;

  grpc::Status SetLogFlushPeriod(grpc::ServerContext* context [[maybe_unused]],
                                 const LogFlushPeriod* request,
                                 ::google::protobuf::Empty*) override;

  grpc::Status SendBench(grpc::ServerContext* context,
                         const com::centreon::engine::BenchParam* request,
                         google::protobuf::Empty* response) override;
};

}
#endif /* !CCE_ENGINERPC_ENGINE_IMPL_HH */
