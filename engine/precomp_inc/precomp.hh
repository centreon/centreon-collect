/**
 * Copyright 2022-2024 Centreon
 *
 * This file is part of Centreon Engine.
 *
 * Centreon Engine is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * Centreon Engine is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Centreon Engine. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef CCE_PRECOMP_HH
#define CCE_PRECOMP_HH

#include <absl/strings/numbers.h>
#include <absl/strings/str_split.h>
#include <absl/strings/string_view.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <asio.hpp>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <deque>
#include <exception>
#include <functional>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <ostream>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <re2/re2.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/circular_buffer.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/optional.hpp>

#include <nlohmann/json.hpp>

#include "com/centreon/engine/namespace.hh"

namespace fmt {

template <>
struct formatter<absl::string_view> : formatter<fmt::string_view> {
  template <typename FormatContext>
  auto format(const absl::string_view& p, FormatContext& ctx) const
      -> decltype(ctx.out()) {
    return formatter<fmt::string_view>::format(
        fmt::string_view(p.data(), p.length()), ctx);
  }
};

}  // namespace fmt

#endif  // CCE_PRECOMP_HH
