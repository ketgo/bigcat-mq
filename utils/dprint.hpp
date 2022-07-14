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

#ifndef UTILS_DPRINT_HPP
#define UTILS_DPRINT_HPP

#include <mutex>
#include <iostream>
#include <thread>

namespace utils {

/**
 * @brief The class `ScopedStream` streams given input to stdout scoped by a
 * mutex.
 *
 */
class ScopedStream {
 public:
  /**
   * @brief Create a scoped stream object.
   *
   */
  static ScopedStream Create();

  /**
   * @brief Print the given input to standard output.
   *
   * @tparam T The type of trace event.
   * @param event Constant reference to the event.
   * @returns Reference to the scoped trace.
   */
  template <class T>
  ScopedStream& operator<<(const T& event);

  /**
   * @brief Destroy the Scoped Trace object
   *
   */
  ~ScopedStream();

 private:
  /**
   * @brief Construct a new ScopedTrace object.
   *
   * @param mutex Reference to the mutex.
   */
  ScopedStream(std::mutex& mutex);

  std::unique_lock<std::mutex> lock_;
};

// ------------------------------------
// ScopedStream Implementation
// ------------------------------------

ScopedStream::ScopedStream(std::mutex& mutex) : lock_(mutex) {
  std::cout << std::this_thread::get_id() << ": ";
}

// --------- public ----------------

// static
ScopedStream ScopedStream::Create() {
  static std::mutex mutex;
  return {mutex};
}

template <class T>
ScopedStream& ScopedStream::operator<<(const T& event) {
  std::cout << event;
  return *this;
}

ScopedStream::~ScopedStream() { std::cout << "\n"; }

// ------------------------------------

}  // namespace utils

#define DPRINT utils::ScopedStream::Create()

#endif /* PRINT_HPP */
