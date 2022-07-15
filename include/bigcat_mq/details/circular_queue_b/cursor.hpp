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

#ifndef BIGCAT_MQ__DETAILS__CIRCULAR_QUEUE_B__CURSOR_HPP
#define BIGCAT_MQ__DETAILS__CIRCULAR_QUEUE_B__CURSOR_HPP

#include <atomic>
#include <cassert>

namespace bigcat {
namespace details {
namespace circular_queue_b {

// ============================================================================

/**
 * @brief Circular queue cursor.
 *
 * The cursor represents a location to an object inside the circular queue. A 64
 * bit data structure is used to store the following values:
 *
 *  a. [Bit 0]: Overflow sign of the cursor. The overflow sign is used to check
 *              for the overflow phase. Upon overflow the sign is flipped.
 *  b. [Bits 1-63]: Location value of the cursor used to get the index of an
 *                  object in the circular queue.
 *
 * @note The cursor is restricted to 64 bits in order to make the atomic type
 * `std::atomic<Cursor>` lock free.
 *
 */
class Cursor {
 public:
  /**
   * @brief Construct a new Cursor object.
   *
   */
  Cursor() = default;

  /**
   * @brief Construct a new Cursor object.
   *
   * @param overflow Overflow sign.
   * @param location Cursor location.
   */
  Cursor(const bool overflow, const std::size_t location);

  /**
   * @brief Get the overflow sign of the cursor.
   *
   * @returns Boolean value representing the overflow sign of the cursor.
   */
  bool Overflow() const;

  /**
   * @brief Get the location value stored in the cursor.
   *
   * @returns The location value.
   */
  uint64_t Location() const;

  /**
   * @brief Operator to check if the given cursor is ahead to this cursor.
   *
   * @param cursor Constant reference to the cursor.
   * @return `true` if ahead else `false`.
   */
  bool operator<(const Cursor &cursor) const;

  /**
   * @brief Operator to check if the given cursor is ahead or equal to this
   * cursor.
   *
   * @param cursor Constant reference to the cursor.
   * @return `true` if ahead or equal else `false`.
   */
  bool operator<=(const Cursor &cursor) const;

  /**
   * @brief Add a value to the location stored in the cursor to get a new
   * cursor.
   *
   * @param offset Value to add.
   * @returns Cursor containing updated location value.
   */
  Cursor operator+(const std::size_t value) const;

  /**
   * @brief Subtract a value to the location stored in the cursor to get a new
   * cursor.
   *
   * @param offset Value to subtract.
   * @returns Cursor containing updated location value.
   */
  Cursor operator-(const std::size_t value) const;

 private:
  /**
   * @brief Flip the overflow sign of the cursor.
   *
   */
  void FlipOverflow();

  bool overflow_ : 1;
  uint64_t location_ : 63;
};

// ---------------------------------------
// Cursor Implementation
// ---------------------------------------

Cursor::Cursor(const bool overflow, const std::size_t location)
    : overflow_(overflow), location_(location) {}

bool Cursor::Overflow() const { return overflow_; }

void Cursor::FlipOverflow() { overflow_ = !overflow_; }

uint64_t Cursor::Location() const { return location_; }

bool Cursor::operator<(const Cursor &cursor) const {
  return overflow_ == cursor.overflow_ ? location_ < cursor.location_
                                       : location_ > cursor.location_;
}

bool Cursor::operator<=(const Cursor &cursor) const {
  return overflow_ == cursor.overflow_ ? location_ <= cursor.location_
                                       : location_ > cursor.location_;
}

Cursor Cursor::operator+(const std::size_t value) const {
  Cursor rvalue(overflow_, location_ + value);
  if (rvalue.location_ < location_) {
    rvalue.FlipOverflow();
  }
  return rvalue;
}

Cursor Cursor::operator-(const std::size_t value) const {
  Cursor rvalue(overflow_, location_ - value);
  if (rvalue.location_ > location_) {
    rvalue.FlipOverflow();
  }
  return rvalue;
}

// ============================================================================

/**
 * @brief Lock free atomic cursor.
 *
 */
using AtomicCursor = std::atomic<Cursor>;

// ============================================================================

}  // namespace circular_queue_b
}  // namespace details
}  // namespace bigcat

#endif /* BIGCAT_MQ__DETAILS__CIRCULAR_QUEUE_B__CURSOR_HPP */
