/*
** Copyright 2007-2008 Ethan Galstad
** Copyright 2007,2010 Andreas Ericsson
** Copyright 2010      Max Schubert
** Copyright 2011-2013 Merethis
**
** This file is part of Centreon Engine.
**
** Centreon Engine is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Engine is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Engine. If not, see
** <http://www.gnu.org/licenses/>.
*/

#ifndef CCE_COMPATIBILITY_CIRCULAR_BUFFER_HH
#define CCE_COMPATIBILITY_CIRCULAR_BUFFER_HH

#include <boost/circular_buffer.hpp>
#include <boost/optional.hpp>

template <class T>
class circular_buffer : protected boost::circular_buffer<T> {
  mutable std::mutex _protect;
  using base_class = boost::circular_buffer<T>;

  size_t _high;

 public:
  circular_buffer();
  void push(const T& to_push);
  boost::optional<T> pop();

  void set_capacity(size_t capacity);
  void clear();
  bool full() const;
  bool empty() const;
  size_t size() const { return base_class::size(); }
  size_t high() const { return _high; }
};

template <class T>
circular_buffer<T>::circular_buffer() : _high(0) {}

template <class T>
void circular_buffer<T>::push(const T& to_push) {
  std::lock_guard<std::mutex> l(_protect);
  base_class::push_back(to_push);
  if (base_class::size() > _high) {
    _high = base_class::size();
  }
}

template <class T>
boost::optional<T> circular_buffer<T>::pop() {
  std::lock_guard<std::mutex> l(_protect);
  if (base_class::empty()) {
    return boost::none;
  }
  T ret(*base_class::begin());
  base_class::pop_front();
  return ret;
}

template <class T>
void circular_buffer<T>::clear() {
  std::lock_guard<std::mutex> l(_protect);
  base_class::clear();
}

template <class T>
bool circular_buffer<T>::full() const {
  std::lock_guard<std::mutex> l(_protect);
  return base_class::full();
}

template <class T>
bool circular_buffer<T>::empty() const {
  std::lock_guard<std::mutex> l(_protect);
  return base_class::empty();
}

template <class T>
void circular_buffer<T>::set_capacity(size_t new_capacity) {
  std::lock_guard<std::mutex> l(_protect);
  if (new_capacity != base_class::capacity()) {
    base_class::set_capacity(new_capacity);
  }
}

#endif  // !CCE_COMPATIBILITY_CIRCULAR_BUFFER_HH
