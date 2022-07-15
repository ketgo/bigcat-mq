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

#ifndef BIGCAT_MQ__DETAILS__RANDOM_HPP
#define BIGCAT_MQ__DETAILS__RANDOM_HPP

#include <cstddef>

namespace bigcat {
namespace details {

/**
 * @brief Thread safe fast pseudo random number generator.
 *
 * The method implements a Xorshoft random number generator with a period of
 * 2^96-1.
 * https://stackoverflow.com/questions/1640258/need-a-fast-random-generator-for-c
 *
 */
inline std::size_t Random() {
  thread_local std::size_t x = 123456789, y = 362436069, z = 521288629;
  x ^= x << 16;
  x ^= x >> 5;
  x ^= x << 1;
  auto t = x;
  x = y;
  y = z;
  z = t ^ x ^ y;
  return z;
}

}  // namespace details
}  // namespace bigcat

#endif /* BIGCAT_MQ__DETAILS__RANDOM_HPP */
