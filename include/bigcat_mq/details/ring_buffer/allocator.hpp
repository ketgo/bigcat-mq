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

#ifndef BIGCAT_MQ__DETAILS__RING_BUFFER__ALLOCATOR_HPP
#define BIGCAT_MQ__DETAILS__RING_BUFFER__ALLOCATOR_HPP

#include <cassert>
#include <type_traits>

#include <bigcat_mq/details/ring_buffer/cursor.hpp>

#include "utils/debug.hpp"

namespace bigcat {
namespace details {
namespace ring_buffer {

// ============================================================================

/**
 * @brief Data structure representing a memory block in the ring buffer.
 *
 * @tparam T Type of objects stored in the memory block.
 */
template <class T>
struct __attribute__((packed)) MemoryBlock {
  // Ensures at compile time that the parameter T has trivial memory layout.
  static_assert(std::is_trivial<T>::value,
                "The data type used does not have a trivial memory layout.");

  size_t size;
  T data[0];
};

// ============================================================================

/**
 * @brief Handle to an allocated memory block in the ring buffer used for
 * reading or writing.
 *
 * @param T Type of objects stored in the memory block.
 * @tparam CursorPool The type of cursor pool.
 */
template <class T, class CursorPool>
class MemoryBlockHandle {
 public:
  /**
   * @brief Construct a new Memory Block Handle object.
   *
   * @param block Pointer to the memory block.
   * @param cursor Pointer to cursor associated with the memory block.
   * @param pool Pointer to cursor pool to which the above curser should be
   * released.
   */
  MemoryBlockHandle(MemoryBlock<T>* block = nullptr, Cursor* cursor = nullptr,
                    CursorPool* pool = nullptr);

  /**
   * @brief Get the number of objects of type T stored in the memory block.
   *
   */
  size_t Size() const;

  /**
   * @brief Get the pointer to the first T type object in the memory block.
   *
   */
  T* Data() const;

  /**
   * @brief Get object at the given index in the memory block.
   *
   * @param n Index value.
   * @returns Reference to the object.
   */
  T& operator[](size_t n) const;

  /**
   * @brief Check if the handle is valid.
   *
   */
  operator bool() const;

 private:
  MemoryBlock<T>* block_;
  CursorGuard<CursorPool> guard_;
};

// --------------------------------
// MemoryBlockHandle Implementation
// --------------------------------

template <class T, class CursorPool>
MemoryBlockHandle<T, CursorPool>::MemoryBlockHandle(MemoryBlock<T>* block,
                                                    Cursor* cursor,
                                                    CursorPool* pool)
    : block_(block), guard_(cursor, pool) {}

template <class T, class CursorPool>
size_t MemoryBlockHandle<T, CursorPool>::Size() const {
  return block_->size;
}

template <class T, class CursorPool>
T* MemoryBlockHandle<T, CursorPool>::Data() const {
  return block_->data;
}

template <class T, class CursorPool>
T& MemoryBlockHandle<T, CursorPool>::operator[](size_t n) const {
  assert(n < block_->size);
  return block_->data[n];
}

template <class T, class CursorPool>
MemoryBlockHandle<T, CursorPool>::operator bool() const {
  return block_ != nullptr;
}

// ============================================================================

/**
 * @brief Allocator to allocate read and write memory blocks in the ring buffer.
 *
 * @tparam T The type of object stored in ring buffer.
 * @tparam BUFFER_SIZE The size of memory buffer in bytes.
 * @tparam MAX_PRODUCERS The maximum number of producers.
 * @tparam MAX_CONSUMERS The maximum number of consumers.
 */
template <class T, size_t BUFFER_SIZE, size_t MAX_PRODUCERS,
          size_t MAX_CONSUMERS>
class Allocator {
  using block_t = MemoryBlock<T>;
  using const_block_t = MemoryBlock<const T>;

 public:
  /**
   * @brief Construct a new Allocator object.
   *
   */
  Allocator();

  /**
   * @brief Allocate memory in the ring buffer for writing.
   *
   * @param size Number of objects to write.
   * @param max_attempt The maximum number of attempts.
   * @returns Pointer to the memory block.
   */
  MemoryBlockHandle<T, CursorPool<MAX_PRODUCERS>> Allocate(const size_t size,
                                                           size_t max_attempt);

  /**
   * @brief Allocate memory in the ring buffer for reading.
   *
   * @param max_attempt The maximum number of attempts.
   * @returns Pointer to the memory block.
   */
  MemoryBlockHandle<const T, CursorPool<MAX_CONSUMERS>> Allocate(
      size_t max_attempt) const;

#ifndef NDEBUG
  /**
   * @brief Get the ring buffer data.
   *
   */
  const unsigned char* Data() const { return data_; }
#endif

 private:
  unsigned char data_[BUFFER_SIZE];              // data buffer
  CursorPool<MAX_PRODUCERS> write_pool_;         // write cursor pool
  Cursor write_head_;                            // write head
  mutable CursorPool<MAX_CONSUMERS> read_pool_;  // read cursor pool
  mutable Cursor read_head_;                     // read head
};

// -------------------------
// Allocator Implementation
// -------------------------

template <class T, size_t BUFFER_SIZE, size_t MAX_PRODUCERS,
          size_t MAX_CONSUMERS>
Allocator<T, BUFFER_SIZE, MAX_PRODUCERS, MAX_CONSUMERS>::Allocator()
    : write_head_(0), read_head_(0) {}

template <class T, size_t BUFFER_SIZE, size_t MAX_PRODUCERS,
          size_t MAX_CONSUMERS>
MemoryBlockHandle<T, CursorPool<MAX_PRODUCERS>>
Allocator<T, BUFFER_SIZE, MAX_PRODUCERS, MAX_CONSUMERS>::Allocate(
    const size_t size, size_t max_attempt) {
  // Get the size of memory block to allocate for writing
  auto block_size = sizeof(block_t) + size * sizeof(T);
  assert(block_size < BUFFER_SIZE);

  // Attempt to get a write cursor from the cursor pool
  auto* cursor = write_pool_.Allocate(max_attempt);
  if (!cursor) {
    return {};
  }
  // Attempt to allocate the requested size of space in the buffer for writing.
  while (max_attempt) {
    size_t start_idx = write_head_.load(std::memory_order_seq_cst);
    size_t end_idx = start_idx + block_size;
    // Allocate chunk only if the end_idx is behind all the allocated read
    // cursors
    if (!read_pool_.WithinBounds(BUFFER_SIZE, end_idx)) {
      cursor->store(start_idx, std::memory_order_seq_cst);
      // Set write head to new value if its original value has not been already
      // changed by another writer.
      if (write_head_.compare_exchange_weak(start_idx, end_idx)) {
        auto* block =
            reinterpret_cast<block_t*>(&data_[start_idx % BUFFER_SIZE]);
        block->size = size;
        return {block, cursor, &write_pool_};
      }
      // Another writer allocated memory before us so try again until success or
      // max attempt is reached.
    }
    // Not enough space to allocate memory so try again until enough space
    // becomes available or the max attempt is reached.
    --max_attempt;
  }
  // Could not allocate the requested space in the specified number of attempts
  // so release the allocated cursor back to the pool.
  write_pool_.Release(cursor);

  return {};
}

template <class T, size_t BUFFER_SIZE, size_t MAX_PRODUCERS,
          size_t MAX_CONSUMERS>
MemoryBlockHandle<const T, CursorPool<MAX_CONSUMERS>>
Allocator<T, BUFFER_SIZE, MAX_PRODUCERS, MAX_CONSUMERS>::Allocate(
    size_t max_attempt) const {
  // Attempt to get a read cursor from the cursor pool
  auto* cursor = read_pool_.Allocate(max_attempt);
  if (!cursor) {
    return {};
  }
  // Attempt to allocate the requested size of space in the buffer for reading.
  while (max_attempt) {
    size_t start_idx = read_head_.load(std::memory_order_seq_cst);
    auto* block = reinterpret_cast<const_block_t*>(
        const_cast<unsigned char*>(&data_[start_idx % BUFFER_SIZE]));
    size_t end_idx = start_idx + block->size;

    // Allocate chunk only if the end_idx is behind all the allocated write
    // cursors
    if (!write_pool_.WithinBounds(BUFFER_SIZE, end_idx)) {
      cursor->store(start_idx, std::memory_order_seq_cst);
      // Set read head to new value if its original value has not been already
      // changed by another reader.
      if (read_head_.compare_exchange_weak(start_idx, end_idx)) {
        DPRINT("ALLOCATE: " + std::to_string(block->size));
        return {block, cursor, &read_pool_};
      }
      // Another reader allocated memory before us so try again until success or
      // max attempt is reached.
    }
    // Not enough space to allocate memory so try again until enough space
    // becomes available or the max attempt is reached.
    --max_attempt;
  }
  // Could not allocate the requested space in the specified number of attempts
  // so release the allocated cursor back to the pool.
  read_pool_.Release(cursor);

  return {};
}

// ============================================================================

}  // namespace ring_buffer
}  // namespace details
}  // namespace bigcat

#endif /* BIGCAT_MQ__DETAILS__RING_BUFFER__ALLOCATOR_HPP */
