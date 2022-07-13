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

#ifndef BIGCAT_MQ__DETAILS__EXPERIMENTAL__CIRCULAR_QUEUE__CURSOR_HPP
#define BIGCAT_MQ__DETAILS__EXPERIMENTAL__CIRCULAR_QUEUE__CURSOR_HPP

#include <atomic>
#include <cassert>

namespace bigcat {
namespace details {
namespace experimental {
namespace circular_queue {

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
   * @brief Flip the overflow sign of the cursor.
   *
   */
  void FlipOverflow();

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
   * @brief Set the location value stored in the cursor.
   *
   * @param value The location value.
   */
  void SetLocation(const uint64_t value);

 private:
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

void Cursor::SetLocation(const uint64_t value) { location_ = value; }

bool Cursor::operator<(const Cursor &cursor) const {
  return overflow_ == cursor.overflow_ ? location_ < cursor.location_
                                       : location_ > cursor.location_;
}

bool Cursor::operator<=(const Cursor &cursor) const {
  return overflow_ == cursor.overflow_ ? location_ <= cursor.location_
                                       : location_ >= cursor.location_;
}

Cursor Cursor::operator+(const std::size_t value) const {
  Cursor rvalue(overflow_, location_ + value);
  if (rvalue.location_ < location_) {
    rvalue.FlipOverflow();
  }
  return rvalue;
}

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
  CursorHandle(std::atomic<Cursor> &cursor, CursorPool &pool);

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
  std::atomic<Cursor> &operator*() const;

  /**
   * @brief Reference operator
   *
   */
  std::atomic<Cursor> *operator->() const;

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

  std::atomic<Cursor> *cursor_;
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
CursorHandle<CursorPool>::CursorHandle(std::atomic<Cursor> &cursor,
                                       CursorPool &pool)
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
std::atomic<Cursor> &CursorHandle<CursorPool>::operator*() const {
  return *cursor_;
}

template <class CursorPool>
std::atomic<Cursor> *CursorHandle<CursorPool>::operator->() const {
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

/**
 * @brief A lock-free and wait-free pool of cursors used by the circular queue
 * for read/write operations. The pool also contains the head cursor which
 * contains the read/write head location.
 *
 * @tparam POOL_SIZE Number of cursors in the pool.
 */
template <std::size_t POOL_SIZE>
class CursorPool {
  friend class CursorHandle<CursorPool>;

 public:
  /**
   * @brief Thread safe fast pseudo random number generator.
   *
   */
  static std::size_t Random();

  /**
   * @brief Construct a new CursorPool object.
   *
   */
  CursorPool();

  /**
   * @brief Get the head cursor.
   *
   * @returns Reference to the head cursor.
   */
  std::atomic<Cursor> &Head();

  // TODO: Move back to IsBehind from IsBehindOrEqual.

  /**
   * @brief Check if the given cursor is behind all the allocated cursors in the
   * pool or equals the head cursor.
   *
   * @returns `true` if behind or equal else `false`.
   */
  bool IsBehindOrEqual(const Cursor &cursor) const;

  /**
   * @brief Check if the given cursor is ahead of all the allocated cursors in
   * the pool or equals the head cursor.
   *
   * @returns `true` if ahead or equal else `false`.
   */
  bool IsAheadOrEqual(const Cursor &cursor) const;

  /**
   * @brief Allocate a cursor from available set of free cursors.
   *
   * The method attempts to allocate a free cursor by random selection. It
   * performs `max_attempt` number of attempts. If no cursor is found then a
   * null pointer is returned.
   *
   * @param max_attempt Maximum number of attempts to perform.
   * @returns Pointer to the allocated cursor.
   */
  CursorHandle<CursorPool> Allocate(std::size_t max_attempt);

 private:
  /**
   * @brief Release an allocated cursor.
   *
   * @param cursor Pointer to the cursor.
   */
  void Release(std::atomic<Cursor> *cursor);

  /**
   * @brief Enumerated states of a cursor.
   *
   */
  enum class CursorState {
    FREE,       // Cursor free for use.
    ALLOCATED,  // Cursor in use.
  };
  std::atomic<CursorState> cursor_state_[POOL_SIZE];
  std::atomic<Cursor> cursor_[POOL_SIZE];
  std::atomic<Cursor> head_;
};

// -----------------------------
// CursorPool Implementation
// -----------------------------

template <std::size_t POOL_SIZE>
void CursorPool<POOL_SIZE>::Release(std::atomic<Cursor> *cursor) {
  assert(cursor != nullptr);
  const std::size_t idx = cursor - cursor_;
  cursor_state_[idx].store(CursorState::FREE, std::memory_order_seq_cst);
}

// ------- public --------------

// static
template <std::size_t POOL_SIZE>
std::size_t CursorPool<POOL_SIZE>::Random() {
  // Implementation of a Xorshoft generator with a period of 2^96-1.
  // https://stackoverflow.com/questions/1640258/need-a-fast-random-generator-for-c
  thread_local size_t x = 123456789, y = 362436069, z = 521288629;
  x ^= x << 16;
  x ^= x >> 5;
  x ^= x << 1;
  auto t = x;
  x = y;
  y = z;
  z = t ^ x ^ y;
  return z;
}

template <std::size_t POOL_SIZE>
CursorPool<POOL_SIZE>::CursorPool() {
  // Inital cursor value
  Cursor cursor(false, 0);
  head_.store(cursor, std::memory_order_seq_cst);
  for (std::size_t idx = 0; idx < POOL_SIZE; ++idx) {
    cursor_[idx].store(cursor, std::memory_order_seq_cst);
    cursor_state_[idx].store(CursorState::FREE, std::memory_order_seq_cst);
  }
}

template <std::size_t POOL_SIZE>
std::atomic<Cursor> &CursorPool<POOL_SIZE>::Head() {
  return head_;
}

template <std::size_t POOL_SIZE>
bool CursorPool<POOL_SIZE>::IsBehindOrEqual(const Cursor &cursor) const {
  auto head = head_.load(std::memory_order_seq_cst);
  if (head < cursor) {
    return false;
  }
  for (std::size_t idx = 0; idx < POOL_SIZE; ++idx) {
    if (cursor_state_[idx].load(std::memory_order_seq_cst) ==
        CursorState::ALLOCATED) {
      auto _cursor = cursor_[idx].load(std::memory_order_seq_cst);
      if (_cursor < cursor) {
        return false;
      }
    }
  }
  return true;
}

template <std::size_t POOL_SIZE>
bool CursorPool<POOL_SIZE>::IsAheadOrEqual(const Cursor &cursor) const {
  auto head = head_.load(std::memory_order_seq_cst);
  if (cursor < head) {
    return false;
  }
  for (std::size_t idx = 0; idx < POOL_SIZE; ++idx) {
    if (cursor_state_[idx].load(std::memory_order_seq_cst) ==
        CursorState::ALLOCATED) {
      auto _cursor = cursor_[idx].load(std::memory_order_seq_cst);
      if (cursor < _cursor) {
        return false;
      }
    }
  }
  return true;
}

template <std::size_t POOL_SIZE>
CursorHandle<CursorPool<POOL_SIZE>> CursorPool<POOL_SIZE>::Allocate(
    std::size_t max_attempt) {
  while (max_attempt) {
    const std::size_t idx = Random() % POOL_SIZE;
    auto expected = CursorState::FREE;
    if (cursor_state_[idx].compare_exchange_strong(expected,
                                                   CursorState::ALLOCATED)) {
      return {cursor_[idx], *this};
    }
    --max_attempt;
  }
  return {};
}

// ============================================================================

}  // namespace circular_queue
}  // namespace experimental
}  // namespace details
}  // namespace bigcat

#endif /* BIGCAT_MQ__DETAILS__EXPERIMENTAL__CIRCULAR_QUEUE__CURSOR_HPP */
