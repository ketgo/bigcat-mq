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

#ifndef BIGCAT_MQ__DETAILS__EXPERIMENTAL__RING_BUFFER__ALLOCATOR_HPP
#define BIGCAT_MQ__DETAILS__EXPERIMENTAL__RING_BUFFER__ALLOCATOR_HPP

#include <bigcat_mq/details/experimental/ring_buffer/block.hpp>

namespace bigcat {
namespace details {
namespace experimental {
namespace ring_buffer {

/**
 * @brief Allocator to allocate read and write memory blocks in the ring buffer.
 *
 * @tparam T The type of object stored in ring buffer.
 * @tparam BUFFER_SIZE The size of memory buffer in bytes.
 * @tparam MAX_PRODUCERS The maximum number of producers.
 * @tparam MAX_CONSUMERS The maximum number of consumers.
 */
template <class T, std::size_t BUFFER_SIZE, std::size_t MAX_PRODUCERS,
          std::size_t MAX_CONSUMERS>
class Allocator {
 public:
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

 private:
  unsigned char data_[BUFFER_SIZE];              // data buffer
  CursorPool<MAX_PRODUCERS> write_pool_;         // write cursor pool
  mutable CursorPool<MAX_CONSUMERS> read_pool_;  // read cursor pool
};

// -------------------------
// Allocator Implementation
// -------------------------

template <class T, std::size_t BUFFER_SIZE, std::size_t MAX_PRODUCERS,
          std::size_t MAX_CONSUMERS>
MemoryBlockHandle<T, CursorPool<MAX_PRODUCERS>>
Allocator<T, BUFFER_SIZE, MAX_PRODUCERS, MAX_CONSUMERS>::Allocate(
    const size_t size, size_t max_attempt) {
  // Get the size of memory block to allocate for writing
  auto block_size = sizeof(MemoryBlock<T>) + size * sizeof(T);
  assert(block_size < BUFFER_SIZE);

  // Attempt to get a write cursor from the cursor pool
  auto cursor_h = write_pool_.Allocate(max_attempt);
  if (!cursor_h) {
    return {};
  }
  // Attempt to allocate the requested size of space in the buffer for writing.
  while (max_attempt) {
    auto start = write_pool_.Head().load(std::memory_order_seq_cst);
    auto end = start + block_size;
    // Allocate chunk only if the end cursor is ahead of all the allocated
    // read cursors
    if (read_pool_.IsAheadOrEqual(end)) {
      // Set write head to new value if its original value has not been already
      // changed by another writer.
      if (write_pool_.Head().compare_exchange_weak(start, end)) {
        cursor_h->store(start, std::memory_order_seq_cst);
        auto* block = reinterpret_cast<MemoryBlock<T>*>(
            &data_[start.Location() % BUFFER_SIZE]);
        block->size = size;
        return {*block, std::move(cursor_h)};
      }
      // Another writer allocated memory before us so try again until success or
      // max attempt is reached.
    }
    // Not enough space to allocate memory so try again until enough space
    // becomes available or the max attempt is reached.
    --max_attempt;
  }
  // Could not allocate the requested space in the specified number of attempts
  return {};
}

template <class T, std::size_t BUFFER_SIZE, std::size_t MAX_PRODUCERS,
          std::size_t MAX_CONSUMERS>
MemoryBlockHandle<const T, CursorPool<MAX_CONSUMERS>>
Allocator<T, BUFFER_SIZE, MAX_PRODUCERS, MAX_CONSUMERS>::Allocate(
    size_t max_attempt) const {
  // Attempt to get a read cursor from the cursor pool
  auto cursor_h = read_pool_.Allocate(max_attempt);
  if (!cursor_h) {
    return {};
  }
  // Attempt to allocate the requested size of space in the buffer for reading.
  while (max_attempt) {
    auto start = read_pool_.Head().load(std::memory_order_seq_cst);
    auto* block = reinterpret_cast<MemoryBlock<const T>*>(
        const_cast<unsigned char*>(&data_[start.Location() % BUFFER_SIZE]));
    auto end = start + block->size;
    // Allocate chunk only if the end cursor is behind all the allocated write
    // cursors
    if (write_pool_.IsBehind(end)) {
      // Set read head to new value if its original value has not been already
      // changed by another reader.
      if (read_pool_.Head().compare_exchange_weak(start, end)) {
        cursor_h->store(start, std::memory_order_seq_cst);
        return {*block, std::move(cursor_h)};
      }
      // Another reader allocated memory before us so try again until success or
      // max attempt is reached.
    }
    // Not enough space to allocate memory so try again until enough space
    // becomes available or the max attempt is reached.
    --max_attempt;
  }
  // Could not allocate the requested space in the specified number of attempts
  return {};
}

// -------------------------

}  // namespace ring_buffer
}  // namespace experimental
}  // namespace details
}  // namespace bigcat

#endif /* BIGCAT_MQ__DETAILS__EXPERIMENTAL__RING_BUFFER__ALLOCATOR_HPP */
