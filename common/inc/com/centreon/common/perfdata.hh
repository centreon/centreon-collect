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
  double _critical;
  double _critical_low;
  bool _critical_mode;
  double _max;
  double _min;
  std::string _name;
  std::string _unit;
  double _value;
  data_type _value_type;
  double _warning;
  double _warning_low;
  bool _warning_mode;

 public:
  static std::list<perfdata> parse_perfdata(
      uint32_t host_id,
      uint32_t service_id,
      const char* str,
      const std::shared_ptr<spdlog::logger>& logger);

  perfdata();
  ~perfdata() noexcept = default;

  double critical() const { return _critical; }
  void critical(double c) { _critical = c; }
  double critical_low() const { return _critical_low; }
  void critical_low(double c) { _critical_low = c; }
  bool critical_mode() const { return _critical_mode; }
  void critical_mode(bool val) { _critical_mode = val; }
  double max() const { return _max; }
  void max(double val) { _max = val; }
  double min() const { return _min; }
  void min(double val) { _min = val; }
  const std::string& name() const { return _name; }
  void name(const std::string&& val) { _name = val; }
  void resize_name(size_t new_size);
  const std::string& unit() const { return _unit; }
  void resize_unit(size_t new_size);
  void unit(const std::string&& val) { _unit = val; }
  double value() const { return _value; }
  void value(double val) { _value = val; }
  data_type value_type() const { return _value_type; };
  void value_type(data_type val) { _value_type = val; }
  double warning() const { return _warning; }
  void warning(double val) { _warning = val; }
  double warning_low() const { return _warning_low; }
  void warning_low(double val) { _warning_low = val; }
  bool warning_mode() const { return _warning_mode; }
  void warning_mode(bool val) { _warning_mode = val; }
};

}  // namespace com::centreon::common

bool operator==(com::centreon::common::perfdata const& left,
                com::centreon::common::perfdata const& right);
bool operator!=(com::centreon::common::perfdata const& left,
                com::centreon::common::perfdata const& right);

#endif
