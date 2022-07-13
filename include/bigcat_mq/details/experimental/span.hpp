/**
 * Copyright 2022 Ketan Goyal
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
 */

#ifndef BIGCAT_MQ__DETAILS__EXPERIMENTAL__RING_BUFFER__SPAN_HPP
#define BIGCAT_MQ__DETAILS__EXPERIMENTAL__RING_BUFFER__SPAN_HPP

#include <array>
#include <string>
#include <type_traits>
#include <vector>

namespace bigcat {
namespace details {
namespace experimental {

/**
 * @brief The class `Span` encapsulates a block of contiguous memory.
 *
 * @tparam T Type of objects stored in the memory block.
 */
template <class T>
class Span {
 public:
  /**
   * @brief Construct a new Span object.
   *
   */
  Span() : size_(0), data_(nullptr) {}

  /**
   * @brief Construct a new Span object
   *
   * @param data Constant pointer to the data.
   * @param size Size of the data.
   */
  Span(const T *data, const std::size_t size) : size_(size), data_(data) {}

  /**
   * @brief Construct a new Span object.
   *
   * @tparam N Size of array.
   */
  template <std::size_t N>
  Span(T (&array)[N]) : size_(N), data_(array) {}

  /**
   * @brief Construct a new Span object
   *
   * @tparam N Size of array.
   * @param array Constant reference to the array.
   */
  template <std::size_t N>
  Span(const std::array<T, N> &array)
      : size_(array.size()), data_(array.data()) {}

  /**
   * @brief Construct a new Span object.
   *
   * @param vector Constant reference to the vector.
   */
  Span(const std::vector<T> &vector)
      : size_(vector.size()), data_(vector.data()) {}

  /**
   * @brief Construct a new Span object.
   *
   * @param string Constant reference to the string.
   */
  template <class U = T,
            typename std::enable_if<std::is_same<U, char>::value>::type...>
  Span(const std::string &string)
      : size_(string.size()), data_(string.data()) {}

  /**
   * @brief Get the number of objects of type T stored in the span.
   *
   */
  std::size_t Size() const { return size_; }

  /**
   * @brief Get the pointer to the first T type object in the span.
   *
   */
  const T *Data() const { return data_; }

 private:
  std::size_t size_;
  const T *data_;
};

}  // namespace experimental
}  // namespace details
}  // namespace bigcat

#endif /* BIGCAT_MQ__DETAILS__EXPERIMENTAL__RING_BUFFER__SPAN_HPP */
