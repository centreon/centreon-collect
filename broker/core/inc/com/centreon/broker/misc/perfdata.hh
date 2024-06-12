/**
 * Copyright 2011-2023 Centreon
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

#ifndef CCB_MISC_PERFDATA_HH
#define CCB_MISC_PERFDATA_HH

#include "com/centreon/broker/namespace.hh"

CCB_BEGIN()

namespace misc {
/**
 *  @class perfdata perfdata.hh "com/centreon/broker/misc/perfdata.hh"
 *  @brief Store perfdata values.
 *
 *  Store perfdata values.
 */
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
  int16_t _value_type;
  float _warning;
  float _warning_low;
  bool _warning_mode;

 public:
  perfdata();
  perfdata(const perfdata&) = default;
  perfdata(perfdata&&) = default;
  ~perfdata() noexcept = default;
  perfdata& operator=(perfdata const& pd);
  perfdata& operator=(perfdata&& pd);
  float critical() const noexcept;
  void critical(float c) noexcept;
  float critical_low() const noexcept;
  void critical_low(float c) noexcept;
  bool critical_mode() const noexcept;
  void critical_mode(bool m) noexcept;
  float max() const noexcept;
  void max(float m) noexcept;
  float min() const noexcept;
  void min(float m) noexcept;
  std::string const& name() const noexcept;
  void name(std::string const& n);
  void name(std::string&& n);
  std::string const& unit() const noexcept;
  void unit(std::string const& u);
  void unit(std::string&& u);
  float value() const noexcept;
  void value(float v) noexcept;
  int16_t value_type() const noexcept;
  void value_type(int16_t t) noexcept;
  float warning() const noexcept;
  void warning(float w) noexcept;
  float warning_low() const noexcept;
  void warning_low(float w) noexcept;
  bool warning_mode() const noexcept;
  void warning_mode(bool m) noexcept;
};

/**
 *  Get the value.
 *
 *  @return Metric value.
 */
// Inlined after profiling for performance.
inline float perfdata::value() const noexcept {
  return _value;
}
}  // namespace misc

CCB_END()

bool operator==(com::centreon::broker::misc::perfdata const& left,
                com::centreon::broker::misc::perfdata const& right);
bool operator!=(com::centreon::broker::misc::perfdata const& left,
                com::centreon::broker::misc::perfdata const& right);

#endif  // !CCB_MISC_PERFDATA_HH
