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

#ifndef BIGCAT_MQ__DETAILS__RING_BUFFER__CURSOR_HPP
#define BIGCAT_MQ__DETAILS__RING_BUFFER__CURSOR_HPP

#include <atomic>
#include <cassert>

#include <bigcat_mq/details/ring_buffer/random.hpp>

#include "utils/debug.hpp"

namespace bigcat {
namespace details {
namespace ring_buffer {

// ============================================================================

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

// ============================================================================

/**
 * @brief A pool of cursors used by the ring buffer for read/write operations.
 *
 * The class implements a lock-free and wait-free approach to cursor management.
 * To perform any read/write operations the ring buffer must first acquire a
 * cursor from the pool using the `Allocate` method, and then must release it
 * once done using the `Release` method. Each allocated cursor contains a
 * positional index inside the ring buffer where the read/write operations
 * are to be performed.
 *
 * @tparam POOL_SIZE Number of cursors in the pool.
 */
template <size_t POOL_SIZE>
class CursorPool {
 public:
  /**
   * @brief The class `CursorHandle` exposes an allocated cursor and provides a
   * convenient RAII way for releasing it back to a cursor pool on DTOR.
   *
   * @note The class does not satisfy CopyConstructable and CopyAssignable
   * concepts. However, it does satisfy MoveConstructable and MoveAssignable
   * concepts.
   *
   */
  class CursorHandle {
   public:
    CursorHandle(const CursorHandle &other) = delete;
    CursorHandle &operator=(const CursorHandle &other) = delete;

    /**
     * @brief Construct a new cursor handle object.
     *
     * @param cursor Pointer to the allocated cursor.
     * @param pool Pointer to the cursor pool.
     */
    CursorHandle(Cursor *cursor = nullptr, CursorPool *pool = nullptr);

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
    Cursor &operator*() const;

    /**
     * @brief Reference operator
     *
     */
    Cursor *operator->() const;

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
    Cursor *cursor_;
    CursorPool *pool_;
  };

  /**
   * @brief Construct a new CursorPool object.
   *
   */
  CursorPool();

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
  CursorHandle Allocate(size_t max_attempt);

  /**
   * @brief Check if the given cursor value is within the index range occupied
   * by the allocated cursors of the pool.
   *
   * @note It is assumed that the given cursor value does not change while the
   * check is being performed.
   *
   * @note The method is not atomic since any of the allocated cursors in the
   * pool can become free or their values changed while the check is being
   * performed.
   *
   * @param buffer_size Size of the ring buffer in bytes.
   * @param cursor_value Value of the cursor.
   * @return `true` if within bound else `false`.
   */
  bool WithinBounds(const size_t buffer_size, size_t cursor_value) const;

 private:
  /**
   * @brief Release an allocated cursor.
   *
   * @param cursor Pointer to the cursor.
   */
  void Release(Cursor *cursor);

  /**
   * @brief Enumerated states of a cursor.
   *
   */
  enum class CursorState {
    FREE,       // Cursor free for use.
    ALLOCATED,  // Cursor in use.
  };

  Cursor cursor_[POOL_SIZE];
  std::atomic<CursorState> cursor_state_[POOL_SIZE];
};

// ---------------------------------------
// CursorPool::CursorHandle Implementation
// ---------------------------------------

template <size_t POOL_SIZE>
CursorPool<POOL_SIZE>::CursorHandle::CursorHandle(Cursor *cursor,
                                                  CursorPool *pool)
    : cursor_(cursor), pool_(pool) {}

template <size_t POOL_SIZE>
CursorPool<POOL_SIZE>::CursorHandle::CursorHandle(CursorHandle &&other)
    : cursor_(other.cursor_), pool_(other.pool_) {
  other.pool_ = nullptr;
}

template <size_t POOL_SIZE>
typename CursorPool<POOL_SIZE>::CursorHandle &
CursorPool<POOL_SIZE>::CursorHandle::operator=(CursorHandle &&other) {
  if (this != &other) {
    cursor_ = other.cursor_;
    pool_ = other.pool_;
    other.pool_ = nullptr;
  }

  return *this;
}

template <size_t POOL_SIZE>
Cursor &CursorPool<POOL_SIZE>::CursorHandle::operator*() const {
  return *cursor_;
}

template <size_t POOL_SIZE>
Cursor *CursorPool<POOL_SIZE>::CursorHandle::operator->() const {
  return cursor_;
}

template <size_t POOL_SIZE>
CursorPool<POOL_SIZE>::CursorHandle::operator bool() const {
  return cursor_ != nullptr;
}

template <size_t POOL_SIZE>
CursorPool<POOL_SIZE>::CursorHandle::~CursorHandle() {
  if (pool_) {
    pool_->Release(cursor_);
  }
}

// -------------------------
// CursorPool Implementation
// -------------------------

template <size_t POOL_SIZE>
void CursorPool<POOL_SIZE>::Release(Cursor *cursor) {
  assert(cursor != nullptr);
  const size_t idx = cursor - cursor_;
  cursor_state_[idx].store(CursorState::FREE, std::memory_order_seq_cst);
}

// ------- public -----------

template <size_t POOL_SIZE>
CursorPool<POOL_SIZE>::CursorPool() {
  for (size_t i = 0; i < POOL_SIZE; ++i) {
    cursor_[i].store(0, std::memory_order_seq_cst);
    cursor_state_[i].store(CursorState::FREE, std::memory_order_seq_cst);
  }
}

template <size_t POOL_SIZE>
typename CursorPool<POOL_SIZE>::CursorHandle CursorPool<POOL_SIZE>::Allocate(
    size_t max_attempt) {
  while (max_attempt) {
    const size_t idx = rand() % POOL_SIZE;
    auto expected = CursorState::FREE;
    if (cursor_state_[idx].compare_exchange_strong(expected,
                                                   CursorState::ALLOCATED)) {
      return {&cursor_[idx], this};
    }
    --max_attempt;
  }
  return {nullptr, nullptr};
}

template <size_t POOL_SIZE>
bool CursorPool<POOL_SIZE>::WithinBounds(const size_t buffer_size,
                                         size_t cursor_value) const {
  auto offset = cursor_value % buffer_size;
  auto cycle = cursor_value / buffer_size;

  std::string message = "WithinBounds: [" + std::to_string(cursor_value) +
                        "][" + std::to_string(offset) + ", " +
                        std::to_string(cycle) + "] E {";
  // TODO: Scope for improvement by reducing the number of cursors to perform
  // checks.
  for (size_t idx = 0; idx < POOL_SIZE; ++idx) {
    if (cursor_state_[idx].load(std::memory_order_seq_cst) ==
        CursorState::ALLOCATED) {
      auto _value = cursor_[idx].load(std::memory_order_seq_cst);
      auto _offset = _value % buffer_size;
      auto _cycle = _value / buffer_size;
      message += "[" + std::to_string(_value) + "][" + std::to_string(_offset) +
                 ", " + std::to_string(_cycle) + "] ";
      if (_cycle == cycle && _offset <= offset) {
        message += "} --> True";
        DPRINT(message);
        return true;
      }
    }
  }

  message += "} --> False";
  DPRINT(message);
  return false;
}

// ============================================================================

}  // namespace ring_buffer
}  // namespace details
}  // namespace bigcat

#endif /* BIGCAT_MQ__DETAILS__RING_BUFFER__CURSOR_HPP */
