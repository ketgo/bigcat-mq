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

#ifndef BIGCAT_MQ__DETAILS__CIRCULAR_QUEUE_A__CURSOR_POOL_HPP
#define BIGCAT_MQ__DETAILS__CIRCULAR_QUEUE_A__CURSOR_POOL_HPP

#include <bigcat_mq/details/random.hpp>
#include <bigcat_mq/details/circular_queue_a/cursor.hpp>

namespace bigcat {
namespace details {
namespace circular_queue_a {

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

  /**
   * @brief Enumerated states of a cursor.
   *
   */
  enum class CursorState {
    FREE,       // Cursor free for use.
    ALLOCATED,  // Cursor in use.
  };
  using AtomicCursorState = std::atomic<CursorState>;

 public:
  /**
   * @brief Construct a new CursorPool object.
   *
   */
  CursorPool();

  /**
   * @brief Check if the given cursor is behind all the allocated cursors in the
   * pool.
   *
   * @returns `true` if behind else `false`.
   */
  bool IsBehind(const Cursor &cursor) const;

  /**
   * @brief Check if the given cursor is ahead of all the allocated cursors in
   * the pool.
   *
   * @returns `true` if ahead else `false`.
   */
  bool IsAhead(const Cursor &cursor) const;

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
  void Release(AtomicCursor *cursor);

  AtomicCursorState cursor_state_[POOL_SIZE];
  AtomicCursor cursor_[POOL_SIZE];
};

// -----------------------------
// CursorPool Implementation
// -----------------------------

template <std::size_t POOL_SIZE>
void CursorPool<POOL_SIZE>::Release(AtomicCursor *cursor) {
  assert(cursor != nullptr);
  const std::size_t idx = cursor - cursor_;
  cursor_state_[idx].store(CursorState::FREE, std::memory_order_seq_cst);
}

// ------- public --------------

template <std::size_t POOL_SIZE>
CursorPool<POOL_SIZE>::CursorPool() {
  // Initial cursor value
  Cursor cursor(false, 0);
  for (std::size_t idx = 0; idx < POOL_SIZE; ++idx) {
    cursor_[idx].store(cursor, std::memory_order_seq_cst);
    cursor_state_[idx].store(CursorState::FREE, std::memory_order_seq_cst);
  }
}

template <std::size_t POOL_SIZE>
bool CursorPool<POOL_SIZE>::IsBehind(const Cursor &cursor) const {
  for (std::size_t idx = 0; idx < POOL_SIZE; ++idx) {
    if (cursor_state_[idx].load(std::memory_order_seq_cst) ==
        CursorState::ALLOCATED) {
      auto _cursor = cursor_[idx].load(std::memory_order_seq_cst);
      if (_cursor <= cursor) {
        return false;
      }
    }
  }
  return true;
}

template <std::size_t POOL_SIZE>
bool CursorPool<POOL_SIZE>::IsAhead(const Cursor &cursor) const {
  for (std::size_t idx = 0; idx < POOL_SIZE; ++idx) {
    if (cursor_state_[idx].load(std::memory_order_seq_cst) ==
        CursorState::ALLOCATED) {
      auto _cursor = cursor_[idx].load(std::memory_order_seq_cst);
      if (cursor <= _cursor) {
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
    // TODO: Check if the cursor is stale
    --max_attempt;
  }
  return {};
}

// ============================================================================

}  // namespace circular_queue_a
}  // namespace details
}  // namespace bigcat

#endif /* BIGCAT_MQ__DETAILS__CIRCULAR_QUEUE_A__CURSOR_POOL_HPP */
