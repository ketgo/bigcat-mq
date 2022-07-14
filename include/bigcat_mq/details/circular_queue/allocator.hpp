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

#ifndef BIGCAT_MQ__DETAILS__CIRCULAR_QUEUE__ALLOCATOR_HPP
#define BIGCAT_MQ__DETAILS__CIRCULAR_QUEUE__ALLOCATOR_HPP

#include <bigcat_mq/details/circular_queue/block.hpp>
#include <bigcat_mq/details/circular_queue/cursor_pool.hpp>

namespace bigcat {
namespace details {
namespace circular_queue {

/**
 * @brief Allocator to allocate read and write memory blocks in a circular
 * queue.
 *
 * @tparam T The type of object stored in circular queue.
 * @tparam BUFFER_SIZE The size of memory buffer in bytes.
 * @tparam MAX_PRODUCERS The maximum number of producers.
 * @tparam MAX_CONSUMERS The maximum number of consumers.
 */
template <class T, std::size_t BUFFER_SIZE, std::size_t MAX_PRODUCERS,
          std::size_t MAX_CONSUMERS>
class Allocator {
 public:
  /**
   * @brief Construct a new Allocator object.
   *
   */
  Allocator();

  /**
   * @brief Allocate memory in the circular queue for writing.
   *
   * @param size Number of objects to write.
   * @param max_attempt The maximum number of attempts.
   * @returns Pointer to the memory block.
   */
  MemoryBlockHandle<T, CursorPool<MAX_PRODUCERS>> Allocate(
      const std::size_t size, std::size_t max_attempt);

  /**
   * @brief Allocate memory in the circular queue for reading.
   *
   * @param max_attempt The maximum number of attempts.
   * @returns Pointer to the memory block.
   */
  MemoryBlockHandle<const T, CursorPool<MAX_CONSUMERS>> Allocate(
      std::size_t max_attempt) const;

  /**
   * @brief Get the raw buffer data.
   *
   */
  const unsigned char* Data() const;

 private:
  unsigned char data_[BUFFER_SIZE];              // data buffer
  CursorPool<MAX_PRODUCERS> write_pool_;         // write cursor pool
  std::atomic<Cursor> write_head_;               // write head cursor
  mutable CursorPool<MAX_CONSUMERS> read_pool_;  // read cursor pool
  mutable std::atomic<Cursor> read_head_;        // read head cursor
};

// -------------------------
// Allocator Implementation
// -------------------------

template <class T, std::size_t BUFFER_SIZE, std::size_t MAX_PRODUCERS,
          std::size_t MAX_CONSUMERS>
Allocator<T, BUFFER_SIZE, MAX_PRODUCERS, MAX_CONSUMERS>::Allocator() {
  Cursor cursor(false, 0);
  write_head_.store(cursor, std::memory_order_seq_cst);
  read_head_.store(cursor, std::memory_order_seq_cst);
}

template <class T, std::size_t BUFFER_SIZE, std::size_t MAX_PRODUCERS,
          std::size_t MAX_CONSUMERS>
MemoryBlockHandle<T, CursorPool<MAX_PRODUCERS>>
Allocator<T, BUFFER_SIZE, MAX_PRODUCERS, MAX_CONSUMERS>::Allocate(
    const std::size_t size, std::size_t max_attempt) {
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
    auto read_head = read_head_.load(std::memory_order_seq_cst);
    auto write_head = write_head_.load(std::memory_order_seq_cst);
    if (read_head <= write_head) {
      cursor_h->store(write_head, std::memory_order_seq_cst);
      auto end = write_head + (block_size - 1);
      // Allocate block only if the end cursor is ahead of all the allocated
      // read cursors
      if (read_pool_.IsAhead(end) && read_head < end) {
        // Set write head to new value if its original value has not been
        // already changed by another writer.
        if (write_head_.compare_exchange_weak(write_head, end + 1)) {
          auto* block = reinterpret_cast<MemoryBlock<T>*>(
              &data_[write_head.Location() % BUFFER_SIZE]);
          block->size = size;
          return {*block, std::move(cursor_h)};
        }
        // Another writer allocated memory before us so try again until success
        // or max attempt is reached.
      }
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
    std::size_t max_attempt) const {
  // Attempt to get a read cursor from the cursor pool
  auto cursor_h = read_pool_.Allocate(max_attempt);
  if (!cursor_h) {
    return {};
  }
  // Attempt to allocate the requested size of space in the buffer for reading.
  while (max_attempt) {
    auto read_head = read_head_.load(std::memory_order_seq_cst);
    auto write_head = write_head_.load(std::memory_order_seq_cst);
    // Read only when the read head is behind write head
    if (read_head < write_head) {
      cursor_h->store(read_head, std::memory_order_seq_cst);
      auto* block =
          reinterpret_cast<MemoryBlock<const T>*>(const_cast<unsigned char*>(
              &data_[read_head.Location() % BUFFER_SIZE]));
      auto block_size = sizeof(MemoryBlock<T>) + block->size * sizeof(T);
      auto end = read_head + (block_size - 1);
      // Allocate block only if the end cursor is behind all the allocated write
      // cursors
      if (write_pool_.IsBehind(end) && end < write_head) {
        // Set read head to new value if its original value has not been already
        // changed by another reader.
        if (read_head_.compare_exchange_weak(read_head, end + 1)) {
          return {*block, std::move(cursor_h)};
        }
        // Another reader allocated memory before us so try again until success
        // or max attempt is reached.
      }
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
const unsigned char*
Allocator<T, BUFFER_SIZE, MAX_PRODUCERS, MAX_CONSUMERS>::Data() const {
  return data_;
}

// -------------------------

}  // namespace circular_queue
}  // namespace details
}  // namespace bigcat

#endif /* BIGCAT_MQ__DETAILS__CIRCULAR_QUEUE__ALLOCATOR_HPP */
