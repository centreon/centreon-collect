/*
** Copyright 2018-2021 Centreon
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
**
** For more information : contact@centreon.com
*/

#ifndef CCB_LUA_STREAM_HH
#define CCB_LUA_STREAM_HH

#include <nlohmann/json.hpp>

#include "com/centreon/broker/lua/luabinding.hh"
#include "com/centreon/broker/lua/macro_cache.hh"
#include "com/centreon/broker/misc/variant.hh"

CCB_BEGIN()

namespace lua {

/**
 *  @class stream stream.hh "com/centreon/broker/lua/stream.hh"
 *  @brief lua stream.
 *
 *  Stream events into lua database.
 *  This stream runs its own thread. The write() internal function pushes to
 *  its exposed queue some events.
 *
 *  The stream thread has its own queue. When it is empty, this queue is swapped
 *  with the exposed queue. This allows to pop events from it without locking
 *  any mutex.
 *
 *  In case of the mutex's queue full, the write function is more called. The
 *  muxer waits for events to be acknowledged before sending again new events.
 *  This procedure is done through the flush() function. In this stream, the
 *  flush function sets a flag _flush to true. Then the stream thread gets the
 *  information of a flush call and can call it.
 *
 *  When the flush flag is false, and the queue is empty, if it is not time to
 *  exit, the thread waits for 500ms before rechecking events.
 */
class stream : public io::stream {
  /* Macro cache */
  macro_cache _cache;

  /* The Lua engine */
  luabinding _luabinding;

  /* Logger ID */
  uint32_t _logger_id;

 public:
  stream(std::string const& lua_script,
         std::map<std::string, misc::variant> const& conf_params,
         std::shared_ptr<persistent_cache> const& cache);
  ~stream() noexcept;
  stream& operator=(const stream&) = delete;
  stream(const stream&) = delete;
  bool read(std::shared_ptr<io::data>& d, time_t deadline) override;
  int write(std::shared_ptr<io::data> const& d) override;
  int32_t flush() override;
  int32_t stop() override;
};
}  // namespace lua

CCB_END()

#endif  // !CCB_LUA_STREAM_HH
