/*
** Copyright 2015 Centreon
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

#ifndef CCB_TEST_TIME_POINTS_HH
#define CCB_TEST_TIME_POINTS_HH

#include <ctime>
#include <vector>

namespace com::centreon::broker {

namespace test {
/**
 *  Class used to store time points.
 */
class time_points {
 public:
  time_points();
  time_points(time_points const& other);
  ~time_points();
  time_points& operator=(time_points const& other);
  time_t operator[](int index) const;
  time_t last() const;
  time_t prelast() const;
  void store();

 private:
  std::vector<time_t> _points;
};
}  // namespace test

}

#endif  // !CCB_TEST_TIME_POINTS_HH
