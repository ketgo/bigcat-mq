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

#ifndef BIGCAT_MQ__DETAILS__CIRCULAR_QUEUE_A__CURSOR_HPP
#define BIGCAT_MQ__DETAILS__CIRCULAR_QUEUE_A__CURSOR_HPP

#include <atomic>
#include <cassert>

namespace bigcat {
namespace details {
namespace circular_queue_a {

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
 * `AtomicCursor` lock free.
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

/**
 * @brief The class `CursorHandle` exposes an atomic cursor and provides a
 * convenient RAII way for managing its state.
 *
 * @note The class does not satisfy CopyConstructable and CopyAssignable
 * concepts. However, it does satisfy MoveConstructable and MoveAssignable
 * concepts.
 *
 * @tparam CursorPool The type of cursor pool.
 */
template <class CursorPool>
class CursorHandle {
 public:
  CursorHandle(const CursorHandle &other) = delete;
  CursorHandle &operator=(const CursorHandle &other) = delete;

  /**
   * @brief Construct a new Cursor Handle object.
   *
   */
  CursorHandle();

  /**
   * @brief Construct a new Cursor Handle object.
   *
   * @param cursor Reference to the cursor.
   * @param pool Reference to the cursor pool.
   */
  CursorHandle(AtomicCursor &cursor, CursorPool &pool);

  /**
   * @brief Construct a new cursor handle object.
   *
   * @param other Rvalue reference to other handle.
   */
  CursorHandle(CursorHandle &&other);

  /**
   * @brief Move assign cursor handle.
   *
   * @param other Rvalue reference to other handle.
   * @returns Reference to the handle.
   */
  CursorHandle &operator=(CursorHandle &&other);

  /**
   * @brief Dereference operators
   *
   */
  AtomicCursor &operator*() const;

  /**
   * @brief Reference operator
   *
   */
  AtomicCursor *operator->() const;

  /**
   * @brief Check if handle is valid.
   *
   */
  operator bool() const;

  /**
   * @brief Destroy the Cursor Handle object.
   *
   */
  ~CursorHandle();

 private:
  /**
   * @brief Release the allocated cursor back to the pool.
   *
   */
  void Release();

  AtomicCursor *cursor_;
  CursorPool *pool_;
};

// ------------------------------------
// CursorHandle Implementation
// ------------------------------------

template <class CursorPool>
void CursorHandle<CursorPool>::Release() {
  if (pool_) {
    pool_->Release(cursor_);
  }
}

// ------------- public ---------------

template <class CursorPool>
CursorHandle<CursorPool>::CursorHandle() : cursor_(nullptr), pool_(nullptr) {}

template <class CursorPool>
CursorHandle<CursorPool>::CursorHandle(AtomicCursor &cursor, CursorPool &pool)
    : cursor_(std::addressof(cursor)), pool_(std::addressof(pool)) {}

template <class CursorPool>
CursorHandle<CursorPool>::CursorHandle(CursorHandle &&other)
    : cursor_(other.cursor_), pool_(other.pool_) {
  other.cursor_ = nullptr;
  other.pool_ = nullptr;
}

template <class CursorPool>
CursorHandle<CursorPool> &CursorHandle<CursorPool>::operator=(
    CursorHandle &&other) {
  if (this != &other) {
    Release();
    cursor_ = other.cursor_;
    pool_ = other.pool_;
    other.cursor_ = nullptr;
    other.pool_ = nullptr;
  }
  return *this;
}

template <class CursorPool>
AtomicCursor &CursorHandle<CursorPool>::operator*() const {
  return *cursor_;
}

template <class CursorPool>
AtomicCursor *CursorHandle<CursorPool>::operator->() const {
  return cursor_;
}

template <class CursorPool>
CursorHandle<CursorPool>::operator bool() const {
  return cursor_ != nullptr && pool_ != nullptr;
}

template <class CursorPool>
CursorHandle<CursorPool>::~CursorHandle() {
  Release();
}

// ============================================================================

}  // namespace circular_queue_a
}  // namespace details
}  // namespace bigcat

#endif /* BIGCAT_MQ__DETAILS__CIRCULAR_QUEUE_A__CURSOR_HPP */
