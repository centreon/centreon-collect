#ifndef __TSDB__BENCH__PRECOMP_INC_HH
#define __TSDB__BENCH__PRECOMP_INC_HH

#include <stdlib.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <list>
#include <memory>
#include <queue>
#include <random>
#include <unordered_set>

#include <absl/strings/numbers.h>
#include <absl/strings/str_cat.h>

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE

#include <spdlog/common.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>
// #include <boost/leaf/error.hpp>
// #include <boost/leaf/exception.hpp>
// #include <boost/leaf/handle_errors.hpp>
#include <boost/beast.hpp>
#include <boost/program_options.hpp>
#include <boost/variant.hpp>

using system_clock = std::chrono::system_clock;
using time_point = system_clock::time_point;
using duration = system_clock::duration;

using io_context_ptr = std::shared_ptr<boost::asio::io_context>;
using logger_ptr = std::shared_ptr<spdlog::logger>;

#endif
