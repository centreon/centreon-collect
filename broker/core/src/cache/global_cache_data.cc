/*
** Copyright 2023 Centreon
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

#include "com/centreon/broker/cache/global_cache_data.hh"
#include "com/centreon/broker/log_v2.hh"

using namespace com::centreon::broker::cache;

void global_cache_data::set_metric_info(uint64_t metric_id,
                                        uint64_t index_id,
                                        const absl::string_view& name,
                                        const absl::string_view& unit,
                                        double min,
                                        double max) {
  try {
    absl::WriterMutexLock l(&_protect);

    auto exist = _metric_info->find(metric_id);
    if (exist != _metric_info->end()) {
      metric_info& to_update = exist->second;
      if (*to_update.name != name) {
        to_update.name = &get_metric_name(name);
      }
      if (*to_update.unit != unit) {
        to_update.unit = &get_metric_unit(unit);
      }
      to_update.min = min;
      to_update.max = max;
      to_update.index_id = index_id;
    } else {
      if (_metric_info->capacity() <= _metric_info->size()) {  // need to grow?
        _metric_info->reserve(_metric_info->capacity() + 0x10000);
      }

      metric_info to_insert = {.index_id = index_id,
                               .name = &get_metric_name(name),
                               .unit = &get_metric_unit(unit),
                               .host_name = nullptr,
                               .service_description = nullptr,
                               .min = min,
                               .max = max};
      _metric_info->emplace(metric_id, to_insert);
      (*_index_id_to_metric_id)[index_id] = metric_id;
    }
  } catch (const interprocess::bad_alloc& e) {
    SPDLOG_LOGGER_DEBUG(log_v2::core(), "file full => grow");
    allocation_exception_handler();
    set_metric_info(metric_id, index_id, name, unit, min, max);
  }
}

const metric_info* global_cache_data::get_metric_info(
    uint32_t metric_id) const {
  auto search = _metric_info->find(metric_id);
  if (search != _metric_info->end()) {
    return &search->second;
  }
  return nullptr;
}

void global_cache_data::managed_map(bool create) {
  std::hash<std::string> toto;
  global_cache::managed_map(create);
  _metric_info = _file->find_or_construct<id_to_metric_info>("metric_info")(
      _file->get_segment_manager());
  _metric_name = _file->find_or_construct<dictionnary>("metric_name")(
      _file->get_segment_manager());
  _metric_unit = _file->find_or_construct<dictionnary>("metric_unit")(
      _file->get_segment_manager());
}

const string& global_cache_data::get_from_dictionnary(
    dictionnary& dico,
    const absl::string_view& value) {
  auto exist =
      dico.find(value, string_string_view_hash(), string_string_view_equal());
  if (exist != dico.end()) {
    return *exist;
  }
  return *dico.emplace(value.data(), value.size(), _file->get_segment_manager())
              .first;
}

void store_host(uint64_t host_id, const absl::string_view& host_name) override;

void store_service(uint64_t host_id,
                   uint64_t service_id,
                   const absl::string_view& service_description) override;

void set_index_mapping(uint64_t index_id,
                       uint64_t host_id,
                       uint64_t service_id) override;
