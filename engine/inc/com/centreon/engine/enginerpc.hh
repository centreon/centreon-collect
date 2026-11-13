#ifndef CCE_ENGINERPC_ENGINERPC_HH
#define CCE_ENGINERPC_ENGINERPC_HH

#include <grpcpp/server.h>
#include "engine_impl.hh"

namespace com::centreon::engine {
class enginerpc final {
  engine_impl _service;
  std::unique_ptr<grpc::Server> _server;

 public:
  enginerpc(const std::string& address, uint16_t port);
  enginerpc() = delete;
  enginerpc(const enginerpc&) = delete;
  ~enginerpc() = default;
  static bool is_unix_socket(const std::string& address);
  static std::string listening_address(const std::string& address,
                                       uint16_t port);
  bool listening() const;
  void shutdown();
};

}
#endif /* !CCE_ENGINERPC_ENGINE_IMPL_HH */
