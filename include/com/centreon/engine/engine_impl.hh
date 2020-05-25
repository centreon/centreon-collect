#ifndef CCE_ENGINERPC_ENGINE_IMPL_HH
#define CCE_ENGINERPC_ENGINE_IMPL_HH

#include "com/centreon/engine/namespace.hh"
#include "engine.grpc.pb.h"

CCE_BEGIN()
class engine_impl final : public Engine::Service {
  grpc::Status GetVersion(grpc::ServerContext* context,
                          const ::google::protobuf::Empty* /*request*/,
                          Version* response) override;
  grpc::Status GetStats(grpc::ServerContext* context,
                        const ::google::protobuf::Empty* /*request*/,
                        Stats* response) override;
  grpc::Status ProcessServiceCheckResult(grpc::ServerContext* context,
                                         const Check* request,
                                         CommandSuccess* response) override;
  grpc::Status ProcessHostCheckResult(grpc::ServerContext* context,
                                      const Check* request,
                                      CommandSuccess* response) override;
  grpc::Status NewThresholdsFile(grpc::ServerContext* context,
                                 const ThresholdsFile* request,
                                 CommandSuccess* response) override;
  grpc::Status GetNbrHost(grpc::ServerContext* context, 
						  const ::google::protobuf::Empty*, 
						  GenericValue*) override;
  grpc::Status GetNbrContact(grpc::ServerContext* context,
						     const ::google::protobuf::Empty*, 
							 GenericValue*) override;
  grpc::Status GetNbrService(grpc::ServerContext* context,
						     const ::google::protobuf::Empty*, 
							 GenericValue*) override;
  grpc::Status GetNbrServiceGroup(grpc::ServerContext* context,
						     const ::google::protobuf::Empty*, 
							 GenericValue*) override;
  grpc::Status GetNbrContactGroup(grpc::ServerContext* context,
						     const ::google::protobuf::Empty*, 
							 GenericValue*) override;
  grpc::Status GetNbrHostGroup(grpc::ServerContext* context,
						     const ::google::protobuf::Empty*, 
							 GenericValue*) override;
  grpc::Status GetNbrServiceDependencies(grpc::ServerContext* context,
						     const ::google::protobuf::Empty*, 
							 GenericValue*) override;
  grpc::Status GetNbrHostDependencies(grpc::ServerContext* context,
						     const ::google::protobuf::Empty*, 
							 GenericValue*) override;
};

CCE_END()
#endif /* !CCE_ENGINERPC_ENGINE_IMPL_HH */
