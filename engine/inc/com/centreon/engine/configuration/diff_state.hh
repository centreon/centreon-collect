/*
 * Copyright 2022 Centreon (https://www.centreon.com/)
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

#ifndef CCE_CONFIGURATION_DIFF_STATE_HH
#define CCE_CONFIGURATION_DIFF_STATE_HH
#include <google/protobuf/util/message_differencer.h>
#include "common/configuration/state.pb.h"

namespace com::centreon::engine::configuration {
/**
 *  @class diff_state diff_state.hh
 *
 *  Implementation of the Protobuf Differencer Reporter to create a
 *  diff protobuf message.
 */
class diff_state
    : public ::google::protobuf::util::MessageDifferencer::Reporter {
  DiffState _dstate;

  void _build_path(
      const std::vector<
          google::protobuf::util::MessageDifferencer::SpecificField>&,
      Path* path);
  void _set_value(
      const google::protobuf::Message& message,
      const std::vector<
          google::protobuf::util::MessageDifferencer::SpecificField>&,
      bool left_side,
      Value* value);

 public:
  void ReportAdded(
      const google::protobuf::Message&,
      const google::protobuf::Message&,
      const std::vector<
          google::protobuf::util::MessageDifferencer::SpecificField>&) override;
  void ReportDeleted(
      const google::protobuf::Message&,
      const google::protobuf::Message&,
      const std::vector<
          google::protobuf::util::MessageDifferencer::SpecificField>&) override;
  void ReportModified(
      const google::protobuf::Message&,
      const google::protobuf::Message&,
      const std::vector<
          google::protobuf::util::MessageDifferencer::SpecificField>&) override;
  const DiffState& report() const;
};
}  // namespace com::centreon::engine::configuration

#endif /* !CCE_CONFIGURATION_DIFF_STATE_HH */
