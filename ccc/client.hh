#ifndef _CCC_CLIENT_HH
#define _CCC_CLIENT_HH
#include <grpcpp/channel.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/generic/generic_stub.h>
#include <list>

namespace com {
namespace centreon {
namespace ccc {
class client {
  enum type { CCC_NONE, CCC_BROKER, CCC_ENGINE };
  std::unique_ptr<grpc::GenericStub> _stub;
  type _server;
  grpc::CompletionQueue _cq;
  std::unique_ptr<grpc::ClientContext> _context;

 public:
  client(std::shared_ptr<grpc::Channel> channel);
  std::list<std::string> methods() const;
};
}  // namespace ccc
}  // namespace centreon
}  // namespace com
#endif
