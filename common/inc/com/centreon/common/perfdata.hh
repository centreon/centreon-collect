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

#ifndef CENTREON_COMMON_PERFDATA_HH
#define CENTREON_COMMON_PERFDATA_HH

namespace com::centreon::common {
class perfdata {
 public:
  enum data_type { gauge = 0, counter, derive, absolute, automatic };

 private:
  float _critical;
  float _critical_low;
  bool _critical_mode;
  float _max;
  float _min;
  std::string _name;
  std::string _unit;
  float _value;
  data_type _value_type;
  float _warning;
  float _warning_low;
  bool _warning_mode;

 public:
  static std::list<perfdata> parse_perfdata(
      uint32_t host_id,
      uint32_t service_id,
      const char* str,
      const std::shared_ptr<spdlog::logger>& logger);

  perfdata();
  ~perfdata() noexcept = default;

  float critical() const { return _critical; }
  void critical(float c) { _critical = c; }
  float critical_low() const { return _critical_low; }
  void critical_low(float c) { _critical_low = c; }
  bool critical_mode() const { return _critical_mode; }
  void critical_mode(bool val) { _critical_mode = val; }
  float max() const { return _max; }
  void max(float val) { _max = val; }
  float min() const { return _min; }
  void min(float val) { _min = val; }
  const std::string& name() const { return _name; }
  void name(std::string_view val) { _name = val; }
  void resize_name(size_t new_size);
  const std::string& unit() const { return _unit; }
  void resize_unit(size_t new_size);
  void unit(const std::string&& val) { _unit = val; }
  float value() const { return _value; }
  void value(float val) { _value = val; }
  data_type value_type() const { return _value_type; };
  void value_type(data_type val) { _value_type = val; }
  float warning() const { return _warning; }
  void warning(float val) { _warning = val; }
  float warning_low() const { return _warning_low; }
  void warning_low(float val) { _warning_low = val; }
  bool warning_mode() const { return _warning_mode; }
  void warning_mode(bool val) { _warning_mode = val; }
};

bool operator==(com::centreon::common::perfdata const& left,
                com::centreon::common::perfdata const& right);
bool operator!=(com::centreon::common::perfdata const& left,
                com::centreon::common::perfdata const& right);

}  // namespace com::centreon::common

#endif
