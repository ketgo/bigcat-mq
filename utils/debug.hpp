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

#ifndef UTILS__DEBUG_HPP
#define UTILS__DEBUG_HPP

#ifndef NDEBUG

#include <mutex>
#include <iostream>
#include <thread>

namespace utils {

/**
 * @brief Tracer captures debug trace events published by the persist lib when
 * build in the DEBUG mode.
 *
 */
class Tracer {
 public:
  /**
   * @brief Get class instance to publish trace events.
   *
   * @returns Reference to the tracer.
   */
  static Tracer& Get();

  /**
   * @brief Print the given trace event to standard output.
   *
   * @tparam T The type of trace event.
   * @param event Constant reference to the event.
   * @returns Reference to the tracer.
   */
  template <class T>
  Tracer& operator<<(const T& event);

 private:
  /**
   * @brief Construct a new Tracer object.
   *
   */
  Tracer() = default;

  std::mutex mutex_;
};

// ------------------------------------
// Tracer Implementation
// ------------------------------------

// static
Tracer& Tracer::Get() {
  static Tracer instance;
  return instance;
}

template <class T>
Tracer& Tracer::operator<<(const T& event) {
  std::unique_lock<std::mutex> lock(mutex_);
  std::cout << std::this_thread::get_id() << ": " << event << "\n";
  return *this;
}

// ------------------------------------

}  // namespace utils

// -------------------------------------
// The following macros should be used to
// interface with the trace lib.
// -------------------------------------

/**
 * @brief Print passed argument.
 *
 */
#define DPRINT(x) utils::Tracer::Get() << x;

#else

#define DPRINT(x)

#endif

// -------------------------------------

#endif /* UTILS__DEBUG_HPP */
