// Copyright 2022 Ketan Goyal
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef UTILS__RANDOM_HPP
#define UTILS__RANDOM_HPP

#include <random>

namespace utils {

/**
 * @brief Random number generator.
 *
 * @tparam T The type of number.
 */
template <class T>
class RandomNumberGenerator {
 public:
  /**
   * @brief Construct a new RandomNumberGenerator object.
   *
   * @param min Minimum possible value.
   * @param max Maximum possible value.
   */
  RandomNumberGenerator(const size_t min, const size_t max)
      : eng_(std::random_device{}()), dist_(min, max) {}

  T operator()() { return T{dist_(eng_)}; }

 private:
  std::mt19937_64 eng_;
  std::uniform_int_distribution<size_t> dist_;
};

}  // namespace utils

#endif /* UTILS__RANDOM_HPP */
