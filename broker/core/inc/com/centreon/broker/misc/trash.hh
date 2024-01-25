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

#ifndef CCB_MISC_TRASH_HH
#define CCB_MISC_TRASH_HH

#include <forward_list>

namespace com::centreon::broker {

namespace misc {
/**
 * @brief this class is a delayed trash where objects are thrown on. Then they
 * are dereferenced after a time passed in parameter it s can be used for
 * objects involved in unsafe asynchronous operations where we aren t sure of
 * objects owning by asynchronous layers
 *
 * TODO: if we used boost, replace forward_list by a multi_index_container
 * @tparam T objects to delete
 */
template <class T>
class trash {
 public:
  using element_pointer = std::shared_ptr<T>;

 protected:
  struct element_erase_pair {
    element_erase_pair() {}
    element_erase_pair(const element_pointer& element, time_t time_to_eras)
        : elem(element), time_to_erase(time_to_eras) {}

    element_pointer elem;
    time_t time_to_erase;
  };

  using trash_content = std::forward_list<element_erase_pair>;

  trash_content _content;
  std::mutex _protect;

  void clean();

 public:
  void to_trash(const element_pointer& to_throw, time_t time_to_erase);
  void refresh_time_to_erase(const element_pointer& to_throw,
                             time_t time_to_erase);
  ~trash() noexcept = default;
};

/**
 * @brief dereference perempted objects
 * not mutex protected
 * @tparam T
 */
template <class T>
void trash<T>::clean() {
  time_t now = time(nullptr);
  _content.remove_if([now](const element_erase_pair& toTest) {
    return toTest.time_to_erase < now;
  });
}

template <class T>
void trash<T>::to_trash(const element_pointer& to_throw, time_t time_to_erase) {
  std::unique_lock<std::mutex> l(_protect);
  clean();
  for (element_erase_pair toUpdate : _content) {
    if (toUpdate.elem == to_throw) {
      toUpdate.time_to_erase = time_to_erase;
      return;
    }
  }
  _content.emplace_front(to_throw, time_to_erase);
}

template <class T>
void trash<T>::refresh_time_to_erase(const element_pointer& to_update,
                                     time_t time_to_erase) {
  std::unique_lock<std::mutex> l(_protect);
  clean();
  for (element_erase_pair toUpdate : _content) {
    if (toUpdate.elem == to_update) {
      toUpdate.time_to_erase = time_to_erase;
      return;
    }
  }
}

}  // namespace misc

}

#endif  // !CCB_MISC_TOKENIZER_HH
