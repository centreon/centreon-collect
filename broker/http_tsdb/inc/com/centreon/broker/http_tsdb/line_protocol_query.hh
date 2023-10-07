/*
** Copyright 2022 Centreon
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

#ifndef CCB_HTTP_TSDB_LINE_PROTOCOL_QUERY_HH
#define CCB_HTTP_TSDB_LINE_PROTOCOL_QUERY_HH

#include "bbdo/storage/metric.hh"
#include "bbdo/storage/status.hh"
#include "com/centreon/broker/cache/global_cache.hh"
#include "com/centreon/broker/http_tsdb/column.hh"
#include "com/centreon/broker/http_tsdb/internal.hh"

namespace com::centreon::broker {

namespace http_tsdb {
/**
 *  @class line_protocol_query line_protocol_query.hh
 * "com/centreon/broker/graphite/line_protocol_query.hh"
 *  @brief Query compiling/generation.
 *
 *  This class compiles a query for further uses, generating
 *  the query fast.
 */
class line_protocol_query {
 public:
  enum class data_type { unknown, metric, status };
  typedef void (line_protocol_query::*data_getter)(io::data const&,
                                                   unsigned&,
                                                   std::ostream&) const;
  typedef void (line_protocol_query::*data_escaper)(std::string const&,
                                                    std::ostream&) const;

  line_protocol_query();
  line_protocol_query(const std::string allowed_macros,
                      std::vector<column> const& columns,
                      data_type type,
                      const std::shared_ptr<spdlog::logger>& logger);
  line_protocol_query(line_protocol_query const& other) = delete;
  ~line_protocol_query() = default;
  void escape_key(std::string const& str, std::ostream& is) const;
  void escape_value(std::string const& str, std::ostream& is) const;

  void append_metric(storage::pb_metric const& me,
                     std::string& request_body) const;
  void append_status(storage::pb_status const& st,
                     std::string& request_body) const;

 private:
  void _append_compiled_getter(data_getter getter, data_escaper escaper);
  void _append_compiled_string(std::string const& str,
                               data_escaper escaper = NULL);
  void _compile_scheme(const std::string allowed_macros,
                       const std::string& scheme,
                       data_escaper escaper);
  void _throw_on_invalid(data_type macro_type);

  template <typename T, typename U, T(U::*member)>
  void _get_member(io::data const& d,
                   unsigned& string_index,
                   std::ostream& is) const;
  void _get_string(io::data const& d,
                   unsigned& string_index,
                   std::ostream& is) const;
  void _get_dollar_sign(io::data const& d,
                        unsigned& string_index,
                        std::ostream& is) const;
  uint64_t _get_index_id(io::data const& d) const;
  void _get_index_id(io::data const& d,
                     unsigned& string_index,
                     std::ostream& is) const;
  void _get_host(io::data const& d,
                 unsigned& string_index,
                 std::ostream& is) const;
  uint64_t _get_host_id(io::data const& d) const;
  void _get_host_id(io::data const& d,
                    unsigned& string_index,
                    std::ostream& is) const;
  void _get_service(io::data const& d,
                    unsigned& string_index,
                    std::ostream& is) const;
  cache::host_serv_pair _get_service_id(io::data const& d) const;
  void _get_service_id(io::data const& d,
                       unsigned& string_index,
                       std::ostream& is) const;
  void _get_instance(io::data const& d,
                     unsigned& string_index,
                     std::ostream& is) const;
  void _get_host_group(io::data const& d,
                       unsigned& string_index,
                       std::ostream& is) const;
  void _get_service_group(io::data const& d,
                          unsigned& string_index,
                          std::ostream& is) const;
  void _get_min(io::data const& d,
                unsigned& string_index,
                std::ostream& is) const;
  void _get_max(io::data const& d,
                unsigned& string_index,
                std::ostream& is) const;

  void _get_resource_id(io::data const& d,
                        unsigned& string_index,
                        std::ostream& is) const;

  void _get_tag_host_id(io::data const& d,
                        TagType tag_type,
                        std::ostream& is) const;

  void _get_tag_host_cat_id(io::data const& d,
                            unsigned& string_index [[maybe_unused]],
                            std::ostream& is) const {
    _get_tag_host_id(d, TagType::HOSTCATEGORY, is);
  }

  void _get_tag_host_group_id(io::data const& d,
                              unsigned& string_index [[maybe_unused]],
                              std::ostream& is) const {
    _get_tag_host_id(d, TagType::HOSTGROUP, is);
  }

  void _get_tag_host_name(io::data const& d,
                          TagType tag_type,
                          std::ostream& is) const;

  void _get_tag_host_cat_name(io::data const& d,
                              unsigned& string_index [[maybe_unused]],
                              std::ostream& is) const {
    _get_tag_host_name(d, TagType::HOSTCATEGORY, is);
  }

  void _get_tag_host_group_name(io::data const& d,
                                unsigned& string_index [[maybe_unused]],
                                std::ostream& is) const {
    _get_tag_host_name(d, TagType::HOSTGROUP, is);
  }

  void _get_tag_serv_id(io::data const& d,
                        TagType tag_type,
                        std::ostream& is) const;

  void _get_tag_serv_cat_id(io::data const& d,
                            unsigned& string_index [[maybe_unused]],
                            std::ostream& is) const {
    _get_tag_serv_id(d, TagType::SERVICECATEGORY, is);
  }

  void _get_tag_serv_group_id(io::data const& d,
                              unsigned& string_index [[maybe_unused]],
                              std::ostream& is) const {
    _get_tag_serv_id(d, TagType::SERVICEGROUP, is);
  }

  void _get_tag_serv_name(io::data const& d,
                          TagType tag_type,
                          std::ostream& is) const;

  void _get_tag_serv_cat_name(io::data const& d,
                              unsigned& string_index [[maybe_unused]],
                              std::ostream& is) const {
    _get_tag_serv_name(d, TagType::SERVICECATEGORY, is);
  }

  void _get_tag_serv_group_name(io::data const& d,
                                unsigned& string_index [[maybe_unused]],
                                std::ostream& is) const {
    _get_tag_serv_name(d, TagType::SERVICEGROUP, is);
  }
  void _get_metric_name(io::data const& d,
                        unsigned& string_index,
                        std::ostream& is) const;
  void _get_metric_id(io::data const& d,
                      unsigned& string_index,
                      std::ostream& is) const;
  void _get_metric_value(io::data const& d,
                         unsigned& string_index,
                         std::ostream& is) const;
  void _get_metric_time(io::data const& d,
                        unsigned& string_index,
                        std::ostream& is) const;
  void _get_status_state(io::data const& d,
                         unsigned& string_index,
                         std::ostream& is) const;
  void _get_status_time(io::data const& d,
                        unsigned& string_index,
                        std::ostream& is) const;

  const cache::metric_info* _get_metric_info(io::data const& d) const;

  // Compiled data.
  std::vector<std::pair<data_getter, data_escaper> > _compiled_getters;
  std::vector<std::string> _compiled_strings;

  // Used for generation.
  data_type _type;

  std::shared_ptr<spdlog::logger> _logger;
};
}  // namespace http_tsdb

}  // namespace com::centreon::broker

#endif  // !CCB_HTTP_TSDB_LINE_PROTOCOL_QUERY_HH
