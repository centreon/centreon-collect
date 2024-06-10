/**
 * Copyright 2011-2013 Merethis
 * Copyright 2014-2024 Centreon
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
#ifndef CCE_CONFIGURATION_POINT_2D_HH
#define CCE_CONFIGURATION_POINT_2D_HH

namespace com::centreon::engine::configuration {
class point_2d {
 public:
  point_2d(int x = -1, int y = -1);
  point_2d(point_2d const& right);
  ~point_2d() throw();
  point_2d& operator=(point_2d const& right);
  bool operator==(point_2d const& right) const throw();
  bool operator!=(point_2d const& right) const throw();
  bool operator<(point_2d const& right) const throw();
  int x() const throw();
  int y() const throw();

 private:
  int _x;
  int _y;
};
}  // namespace com::centreon::engine::configuration

#endif  // !CCE_CONFIGURATION_POINT_2D_HH
