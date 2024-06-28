/**
 * Copyright 2024 Centreon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For more information : contact@centreon.com
 *
 */
#ifndef CCE_CONFIGURATION_DATERANGE_HH
#define CCE_CONFIGURATION_DATERANGE_HH

namespace com::centreon::engine::configuration {

class daterange;

/**
 * @brief The timerange configuration object. It is almost the same as the
 * com::centreon::engine::timerange object. But it is needed for the
 * configuration library to be Engine independent.
 */
class timerange {
  uint64_t _range_start;
  uint64_t _range_end;

 public:
  timerange(uint64_t range_start, uint64_t range_end)
      : _range_start{range_start}, _range_end{range_end} {}

  uint64_t range_start() const { return _range_start; };
  uint64_t range_end() const { return _range_end; };
  bool operator==(const timerange& other) const {
    return _range_start == other._range_start && _range_end == other._range_end;
  }
  bool operator<(const timerange& other) const {
    if (_range_start != other._range_start)
      return _range_start < other._range_start;
    return _range_end < other._range_end;
  }
};

/**
 * @brief The daterange configuration object. It is almost the same as the
 * com::centreon::engine::daterange object. But it is needed for the
 * configuration library to be Engine independent.
 */
class daterange {
 public:
  enum type_range {
    none = -1,
    calendar_date = 0,
    month_date = 1,
    month_day = 2,
    month_week_day = 3,
    week_day = 4,
    daterange_types = 5,
  };

 private:
  type_range _type;
  /* Start year. */
  int32_t _syear;

  /* Start month. */
  int32_t _smon;

  /* Start day of month (may 3rd, last day in feb). */
  int32_t _smday;

  /* Start day of week (thursday).*/
  int32_t _swday;

  /* Start weekday offset (3rd thursday, last monday in jan). */
  int32_t _swday_offset;

  /* End year. */
  int32_t _eyear;

  /* End month. */
  int32_t _emon;

  /* End day of month (may 3rd, last day in feb). */
  int32_t _emday;

  /* End day of week (thursday).*/
  int32_t _ewday;

  /* End weekday offset (3rd thursday, last monday in jan). */
  int32_t _ewday_offset;

  int32_t _skip_interval;

  /* A list of timeranges for this daterange */
  std::list<timerange> _timerange;

 public:
  daterange(type_range type,
            int syear,
            int smon,
            int smday,
            int swday,
            int swday_offset,
            int eyear,
            int emon,
            int emday,
            int ewday,
            int ewday_offset,
            int skip_interval);
  daterange(type_range type);
  bool is_date_data_equal(const daterange& range) const;
  type_range type() const;
  int32_t get_syear() const { return _syear; }
  int32_t get_smon() const { return _smon; }
  int32_t get_smday() const { return _smday; }
  int32_t get_swday() const { return _swday; }
  int32_t get_swday_offset() const { return _swday_offset; }
  int32_t get_eyear() const { return _eyear; }
  int32_t get_emon() const { return _emon; }
  int32_t get_emday() const { return _emday; }
  int32_t get_ewday() const { return _ewday; }
  int32_t get_ewday_offset() const { return _ewday_offset; }
  int32_t get_skip_interval() const { return _skip_interval; }
  const std::list<timerange>& get_timerange() const;

  void set_syear(int32_t syear);
  void set_smon(int32_t smon);
  void set_smday(int32_t smday);
  void set_swday(int32_t swday);
  void set_swday_offset(int32_t swday_offset);
  void set_eyear(int32_t eyear);
  void set_emon(int32_t emon);
  void set_emday(int32_t emday);
  void set_ewday(int32_t ewday);
  void set_ewday_offset(int32_t ewday_offset);
  void set_skip_interval(int32_t skip_interval);
  void set_timerange(const std::list<timerange>& timerange);

  bool operator==(const daterange& other) const;
  bool operator<(const daterange& other) const;
};

std::ostream& operator<<(std::ostream& os, const timerange& obj);

std::ostream& operator<<(std::ostream& os, const std::list<timerange>& obj);

std::ostream& operator<<(
    std::ostream& os,
    const std::array<std::list<daterange>, daterange::daterange_types>& obj);

}  // namespace com::centreon::engine::configuration

#endif /* !CCE_CONFIGURATION_DATERANGE_HH */
