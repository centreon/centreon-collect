/**
 * Copyright 2022 Centreon
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

#ifndef CCB_HTTP_TSDB_COLUMN_HH
#define CCB_HTTP_TSDB_COLUMN_HH

namespace com::centreon::broker {

namespace http_tsdb {
/**
 *  @class column column.hh "com/centreon/broker/http_tsdb/column.hh"
 *  @brief Store the data for a column in the query.
 */
class column {
 public:
  enum class type { string, number };

  column();
  column(std::string const& name,
         std::string const& value,
         bool is_tag,
         type col_type);

  std::string const& get_name() const { return _name; }
  std::string const& get_value() const { return _value; }
  bool is_tag() const { return _is_tag; }
  type get_type() const { return _type; }
  static type parse_type(std::string const& type);

 private:
  std::string _name;
  std::string _value;
  bool _is_tag;
  type _type;
};
}  // namespace http_tsdb

}  // namespace com::centreon::broker

#endif  // !CCB_HTTP_TSDB_COLUMN_HH
