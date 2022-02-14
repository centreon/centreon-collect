#include "com/centreon/broker/grpc/channel.hh"
#include "com/centreon/broker/misc/string.hh"
#include "com/centreon/broker/misc/trash.hh"
#include "grpc_stream.pb.h"

using namespace com::centreon::broker::grpc;

static com::centreon::broker::misc::trash<channel> _trash;

namespace com {
namespace centreon {
namespace broker {
namespace stream {
std::ostream& operator<<(std::ostream& st,
                         const centreon_stream::centreon_event& to_dump) {
  if (to_dump.IsInitialized()) {
    if (to_dump.has_raw_data()) {
      const com::centreon::broker::stream::centreon_raw_data& raw_data =
          to_dump.raw_data();
      st << "type:" << raw_data.type() << "src:" << raw_data.source()
         << " dest:" << raw_data.destination();
    }
  }
  return st;
}
}  // namespace stream
namespace grpc {
std::ostream& operator<<(std::ostream& st,
                         const detail_centreon_event& to_dump) {
  if (to_dump.to_dump.IsInitialized()) {
    st << to_dump.to_dump;
    if (to_dump.to_dump.has_raw_data()) {
      const std::string& buff = to_dump.to_dump.raw_data().buffer();
      st << " buff:"
         << com::centreon::broker::misc::string::debug_buf(buff.c_str(),
                                                           buff.length(), 100);
    }
  }
  return st;
}
}  // namespace grpc
}  // namespace broker
}  // namespace centreon
}  // namespace com

constexpr unsigned second_delay_before_delete = 60;

void channel::to_trash() {
  _thrown = true;
  log_v2::grpc()->debug("{} this={:p}", __PRETTY_FUNCTION__,
                        static_cast<void*>(this));
  _trash.to_trash(shared_from_this(),
                  time(nullptr) + second_delay_before_delete);
}
