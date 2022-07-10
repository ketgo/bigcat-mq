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

#ifndef UTILS__CHRONO_HPP
#define UTILS__CHRONO_HPP

#include <chrono>

#include "utils/random.hpp"

namespace utils {

/**
 * @brief Random delay generator.
 *
 * @tparam ChronoUnitType Unit type of the delay.
 */
template <class ChronoUnitType = std::chrono::microseconds>
using RandomDelayGenerator = RandomNumberGenerator<ChronoUnitType>;

}  // namespace utils

#endif /* UTILS__CHRONO_HPP */
