/**
 * Copyright 2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 */
#ifndef CMT_FUNCT_INFO_CACHE_HH
#define CMT_FUNCT_INFO_CACHE_HH

namespace com::centreon::malloc_trace {
/**
 * @brief symbol information research is very expensive
 * so we store function informations in a cache
 *
 */
class funct_info {
  const std::string _funct_name;
  const std::string _source_file;
  const size_t _source_line;

 public:
  funct_info(std::string&& funct_name,
             std::string&& source_file,
             size_t source_line)
      : _funct_name(funct_name),
        _source_file(source_file),
        _source_line(source_line) {}

  const std::string& get_funct_name() const { return _funct_name; }
  const std::string& get_source_file() const { return _source_file; }
  size_t get_source_line() const { return _source_line; }
};

using funct_cache_map =
    std::map<boost::stacktrace::frame::native_frame_ptr_t, funct_info>;

}  // namespace com::centreon::malloc_trace

#endif
