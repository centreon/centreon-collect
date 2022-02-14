#ifndef CCB_GRPC_PRECOMP_HH
#define CCB_GRPC_PRECOMP_HH

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <limits>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <string>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <grpc/grpc.h>
#include <grpcpp/alarm.h>
#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/impl/codegen/client_callback.h>
#include <grpcpp/security/credentials.h>

#include "com/centreon/broker/io/endpoint.hh"
#include "com/centreon/broker/io/raw.hh"
#include "com/centreon/broker/io/stream.hh"
#include "com/centreon/broker/log_v2.hh"
#include "com/centreon/exceptions/msg_fmt.hh"

using system_clock = std::chrono::system_clock;
using time_point = system_clock::time_point;
using duration = system_clock::duration;

#endif  // CCB_GRPC_PRECOMP_HH
