/**
 * Copyright 2024 Centreon
 *
 * This file is part of Centreon Agent.
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

#ifndef CA_PRECOMP_HH
#define CA_PRECOMP_HH

#include <deque>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include <fmt/chrono.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <absl/container/flat_hash_map.h>
#include <absl/strings/str_split.h>

#include <boost/asio.hpp>

namespace asio = boost::asio;

#include <boost/algorithm/string.hpp>
#include <boost/process/v2.hpp>
#include <boost/program_options.hpp>

#endif
