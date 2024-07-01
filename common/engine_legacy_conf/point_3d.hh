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
#ifndef CCE_CONFIGURATION_POINT_3D_HH
#define CCE_CONFIGURATION_POINT_3D_HH

namespace com::centreon::engine {

namespace configuration {
class point_3d {
 public:
  point_3d(double x = 0.0, double y = 0.0, double z = 0.0);
  point_3d(point_3d const& right);
  ~point_3d() throw();
  point_3d& operator=(point_3d const& right);
  bool operator==(point_3d const& right) const throw();
  bool operator!=(point_3d const& right) const throw();
  bool operator<(point_3d const& right) const throw();
  double x() const throw();
  double y() const throw();
  double z() const throw();

 private:
  double _x;
  double _y;
  double _z;
};
}  // namespace configuration

}  // namespace com::centreon::engine

#endif  // !CCE_CONFIGURATION_POINT_3D_HH
