/*
** Copyright 2011-2013 Centreon
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

#ifndef CCB_MISC_PERFDATA_HH
#define CCB_MISC_PERFDATA_HH

namespace com::centreon::broker {

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
  double _critical;
  double _critical_low;
  bool _critical_mode;
  double _max;
  double _min;
  std::string _name;
  std::string _unit;
  double _value;
  int16_t _value_type;
  double _warning;
  double _warning_low;
  bool _warning_mode;

 public:
  perfdata();
  perfdata(const perfdata&) = default;
  perfdata(perfdata&&) = default;
  ~perfdata() noexcept;
  perfdata& operator=(perfdata const& pd);
  perfdata& operator=(perfdata&& pd);
  double critical() const noexcept;
  void critical(double c) noexcept;
  double critical_low() const noexcept;
  void critical_low(double c) noexcept;
  bool critical_mode() const noexcept;
  void critical_mode(bool m) noexcept;
  double max() const noexcept;
  void max(double m) noexcept;
  double min() const noexcept;
  void min(double m) noexcept;
  std::string const& name() const noexcept;
  void name(std::string const& n);
  void name(std::string&& n);
  std::string const& unit() const noexcept;
  void unit(std::string const& u);
  void unit(std::string&& u);
  double value() const noexcept;
  void value(double v) noexcept;
  int16_t value_type() const noexcept;
  void value_type(int16_t t) noexcept;
  double warning() const noexcept;
  void warning(double w) noexcept;
  double warning_low() const noexcept;
  void warning_low(double w) noexcept;
  bool warning_mode() const noexcept;
  void warning_mode(bool m) noexcept;
};

/**
 *  Get the value.
 *
 *  @return Metric value.
 */
// Inlined after profiling for performance.
inline double perfdata::value() const noexcept {
  return _value;
}
}  // namespace misc

}  // namespace com::centreon::broker

bool operator==(com::centreon::broker::misc::perfdata const& left,
                com::centreon::broker::misc::perfdata const& right);
bool operator!=(com::centreon::broker::misc::perfdata const& left,
                com::centreon::broker::misc::perfdata const& right);

#endif  // !CCB_MISC_PERFDATA_HH
