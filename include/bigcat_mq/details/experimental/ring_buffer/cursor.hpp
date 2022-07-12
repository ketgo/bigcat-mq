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

#ifndef BIGCAT_MQ__DETAILS__EXPERIMENTAL__RING_BUFFER__CURSOR_HPP
#define BIGCAT_MQ__DETAILS__EXPERIMENTAL__RING_BUFFER__CURSOR_HPP

#include <atomic>

namespace bigcat {
namespace details {
namespace experimental {
namespace ring_buffer {

/**
 * @brief Ring buffer atomic cursor.
 *
 * The cursor represents a location to a character in a ring buffer. It
 * comprises of an offset from the starting index 0, and the number of cycles
 * completed when traversing the buffer. An atomic size_t type is used to
 * store values of a cursor such that the offset and cycle can be computed using
 * the following equations:
 *
 * offset = cursor_value % buffer_size
 * cycle = cursor_value / buffer_size
 *
 */
using Cursor = std::atomic_size_t;

}  // namespace ring_buffer
}  // namespace experimental
}  // namespace details
}  // namespace bigcat

#endif /* BIGCAT_MQ__DETAILS__EXPERIMENTAL__RING_BUFFER__CURSOR_HPP */
