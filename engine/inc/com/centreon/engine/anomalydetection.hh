/**
 * Copyright 2020-2024 Centreon
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
#ifndef CCE_ANOMALYDETECTION_HH
#define CCE_ANOMALYDETECTION_HH

#include <map>
#include <mutex>

#include <rapidjson/document.h>
#include <tuple>

#include "com/centreon/engine/service.hh"

namespace com::centreon::engine {

class anomalydetection : public service {
 public:
  using pointer_set = std::set<anomalydetection*>;

  class threshold_point {
    time_t _timepoint;
    double _lower, _upper, _fit, _lower_margin, _upper_margin;

    enum class e_format { V1, V2 };

    e_format _format;

   public:
    threshold_point(time_t timepoint,
                    double factor,
                    const rapidjson::Value& json_data);
    threshold_point(time_t timepoint);

    void set_factor(double factor);

    time_t get_timepoint() const { return _timepoint; }
    double get_lower() const { return _lower; }
    double get_upper() const { return _upper; }
    double get_fit() const { return _fit; }
    double get_lower_margin() const { return _lower_margin; }
    double get_upper_margin() const { return _upper_margin; }

    threshold_point interpoll(time_t timepoint,
                              const threshold_point& left) const;
  };

 protected:
  service* _dependent_service;
  uint64_t _internal_id;
  std::string _metric_name;
  std::string _thresholds_file;
  bool _status_change;
  bool _thresholds_file_viable;
  double _sensitivity;
  uint64_t _dependent_service_id;

  using threshold_point_map = std::map<time_t, threshold_point>;
  threshold_point_map _thresholds;
  std::mutex _thresholds_m;

 public:
  anomalydetection(uint64_t host_id,
                   uint64_t service_id,
                   std::string const& hostname,
                   std::string const& description,
                   std::string const& display_name,
                   uint64_t internal_id,
                   service* dependent_service,
                   std::string const& metric_name,
                   std::string const& thresholds_file,
                   bool status_change,
                   bool checks_enabled,
                   bool accept_passive_checks,
                   uint32_t check_interval,
                   uint32_t retry_interval,
                   uint32_t notification_interval,
                   int max_attempts,
                   uint32_t first_notification_delay,
                   uint32_t recovery_notification_delay,
                   std::string const& notification_period,
                   bool notifications_enabled,
                   bool is_volatile,
                   std::string const& event_handler,
                   bool event_handler_enabled,
                   std::string const& notes,
                   std::string const& notes_url,
                   std::string const& action_url,
                   std::string const& icon_image,
                   std::string const& icon_image_alt,
                   bool flap_detection_enabled,
                   double low_flap_threshold,
                   double high_flap_threshold,
                   bool check_freshness,
                   int freshness_threshold,
                   bool obsess_over,
                   std::string const& timezone,
                   uint64_t icon_id,
                   double sensitivity);
  ~anomalydetection();
  uint64_t get_internal_id() const;
  void set_internal_id(uint64_t id);
  service* get_dependent_service() const;
  void set_dependent_service(service* svc);
  void set_metric_name(std::string const& name);
  void set_thresholds_file(std::string const& file);

  void set_thresholds_lock(const std::string& filename,
                           double json_sensitivity,
                           const rapidjson::Value& thresholds);
  void set_thresholds_no_lock(const std::string& filename,
                              double json_sensitivity,
                              const rapidjson::Value& thresholds);

  void set_sensitivity(double sensitivity);
  double get_sensitivity() const { return _sensitivity; }

  static int update_thresholds(const std::string& filename);
  virtual int run_async_check(int check_options,
                              double latency,
                              bool scheduled_check,
                              bool reschedule_check,
                              bool* time_is_valid,
                              time_t* preferred_time) noexcept override;
  int handle_async_check_result(
      const check_result& queued_check_result) override;
  bool parse_perfdata(std::string const& perfdata,
                      time_t check_time,
                      check_result& calculated_result);
  void init_thresholds();
  void set_status_change(bool status_change);
  const std::string& get_metric_name() const;
  const std::string& get_thresholds_file() const;
  void resolve(uint32_t& w, uint32_t& e);

  static const pointer_set& get_anomaly(uint64_t dependent_service_id);
};
}  // namespace com::centreon::engine

com::centreon::engine::anomalydetection* add_anomalydetection(
    uint64_t host_id,
    uint64_t service_id,
    std::string const& host_name,
    std::string const& description,
    std::string const& display_name,
    uint64_t internal_id,
    uint64_t dependent_service_id,
    std::string const& metric_name,
    std::string const& thresholds_file,
    bool status_change,
    int max_attempts,
    double check_interval,
    double retry_interval,
    double notification_interval,
    uint32_t first_notification_delay,
    uint32_t recovery_notification_delay,
    std::string const& notification_period,
    bool notify_recovery,
    bool notify_unknown,
    bool notify_warning,
    bool notify_critical,
    bool notify_flapping,
    bool notify_downtime,
    bool notifications_enabled,
    bool is_volatile,
    std::string const& event_handler,
    bool event_handler_enabled,
    bool checks_enabled,
    bool accept_passive_checks,
    bool flap_detection_enabled,
    double low_flap_threshold,
    double high_flap_threshold,
    bool flap_detection_on_ok,
    bool flap_detection_on_warning,
    bool flap_detection_on_unknown,
    bool flap_detection_on_critical,
    bool stalk_on_ok,
    bool stalk_on_warning,
    bool stalk_on_unknown,
    bool stalk_on_critical,
    int process_perfdata,
    bool check_freshness,
    int freshness_threshold,
    std::string const& notes,
    std::string const& notes_url,
    std::string const& action_url,
    std::string const& icon_image,
    std::string const& icon_image_alt,
    int retain_status_information,
    int retain_nonstatus_information,
    bool obsess_over,
    std::string const& timezone,
    uint64_t icon_id,
    double sensitivity);

#endif  // !CCE_ANOMALYDETECTION_HH
