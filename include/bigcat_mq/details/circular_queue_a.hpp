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

#ifndef BIGCAT_MQ__DETAILS__CIRCULAR_QUEUE_A_HPP
#define BIGCAT_MQ__DETAILS__CIRCULAR_QUEUE_A_HPP

#include <bigcat_mq/details/span.hpp>
#include <bigcat_mq/details/circular_queue_a/allocator.hpp>

// TODO: Release cursors for stale processes which have abruptly died. Note that
// if we reprocess the message block pointed by these cursors we implement the
// AtleastOnce delivery and if we ignore these messages we implement the
// AtmostOnce delivery.

namespace bigcat {
namespace details {

/**
 * @brief The class `CircularQueueTypeATypeA` is a lock-free and wait-free
 * circular queue supporting multiple concurrent consumers and producers.
 *
 * The circular queue is designed to be lock-free and wait-free supporting
 * multiple consumers and producers. It exposes two operations, `Publish` for
 * writing and `Consume` for reading. Details on the two are discussed in the
 * following sections.
 *
 *  *
 * <-------llllll0000000XXXXXXXXXXXXXXXXXXXaaaaaaaaaaabbbbbbbcccccccccc-------->
 *         ^     ^      ^                  ^          ^      ^         ^
 *         |     |      |                  |          |      |         |
 *     read[l] read[0]  read_head         write[a] write[b] write[c]  write_head
 *
 * MemoryBlock
 * -----------
 *
 * Data is written on a ring buffer in the form of `MemoryBlock` data structure.
 * The data structure contains two parts: header and body. The header contains
 * the size of the body and possibly a checksum. The body contains the data
 * published by a producer.
 *
 * Write
 * -----
 *
 * In order to support multiple producers, the ring buffer contains multiple
 * write cursors and a write head. When writing a message of size s, a producer
 * is required to reserves a memory block of size S bytes on the buffer. The
 * following steps are performed for allocation:
 *
 * 1. Try to get a free cursor from the pool of write cursors. If no cursor is
 *    available then return an error. This can also be a blocking call where we
 *    wait till the cursor is available.
 * 2. Check if the buffer has available size greater than S. If not then
 *    release the free cursor and return an error. We can also wait till the
 *    desired size is available. Note that S should never be greater than the
 *    buffer capacity.
 * 3. Attempt to reserve a memory block by storing the write head value in the
 *    obtained cursor and moving the head by S. Moving the head should be
 *    performed by CAS (compare and swap). Here we compare the current value
 *    with the original value of head before we attempted write. If the CAS
 *    operation fails then goto step 2.
 *
 * Once the desired size is allocated, the given message is written on the
 * buffer.
 *
 * Read
 * ----
 *
 * In order to support multiple consumers, the ring buffer contains multiple
 * read cursors and a read head. When reading a message, a consumer is required
 * to request for a read cursor. The following steps are performed for
 * allotment:
 *
 * 1. Try to get a free cursor from the pool of read cursors. If no cursor is
 *    available then return an error. This can also be a blocking call where we
 *    wait till the cursor is available.
 * 2. Check that the read head is behind all the write cursors. If not then
 *    release the free cursor and return an error. We can also wait till the
 *    read head becomes behind all the write cursors.
 * 3. Attempt to reserve a memory block for reading by storing the read head
 *    value in the obtained cursor and moving the head by block size. The block
 *    size is stored in the first 8 bytes. Moving the head should be performed
 *    by CAS (compare and swap). Here we compare the current head value with the
 *    original value before we attempted write. If the CAS operation fails then
 *    goto step 2.
 *
 * Once the block is reserved, the message contained is loaded into a desired
 * variable from the buffer.
 *
 * @tparam T The type of object stored in the circular queue.
 * @tparam BUFFER_SIZE The size of memory buffer in bytes.
 * @tparam MAX_PRODUCERS The maximum number of producers.
 * @tparam MAX_CONSUMERS The maximum number of consumers.
 */
template <class T, std::size_t BUFFER_SIZE, std::size_t MAX_PRODUCERS,
          std::size_t MAX_CONSUMERS>
class CircularQueueTypeA {
 public:
  /**
   * @brief Enumerated set of results.
   *
   */
  enum class Result {
    SUCCESS = 0,             // Successful completion of operation
    ERROR_BUFFER_FULL = 1,   // Queue full error
    ERROR_BUFFER_EMPTY = 2,  // Queue empty error
  };

  /**
   * @brief Span encapsulating memory block in the circular queue for reading.
   *
   */
  using ReadSpan = circular_queue_a::MemoryBlockHandle<
      const T, circular_queue_a::CursorPool<MAX_CONSUMERS>>;

  /**
   * @brief Span encapsulating memory block to write in the circular queue.
   *
   */
  using WriteSpan = Span<T>;

  /**
   * @brief Default maximum number of attempts made when publishing or consuming
   * data from the ring buffer.
   *
   */
  constexpr static std::size_t defaultMaxAttempt() { return 32; }

  /**
   * @brief Publish data to ring buffer.
   *
   * The method copies the data stored in the given memory location onto the
   * ring buffer.
   *
   * @param span Constant reference to the write span.
   * @param max_attempt Maximum number of attempts to perform.
   * @returns Result of the operation.
   */
  Result Publish(const WriteSpan &span,
                 size_t max_attempt = defaultMaxAttempt());

  /**
   * @brief Consume from ring buffer.
   *
   * The method fills the passed span object such that it encapsulates the
   * stored data on the ring buffer for consumption.
   *
   * @param span Reference to the read span.
   * @param max_attempt Maximum number of attempts to perform.
   * @returns Result of the operation.
   */
  Result Consume(ReadSpan &span,
                 size_t max_attempt = defaultMaxAttempt()) const;

  /**
   * @brief Get the raw data in the circular queue.
   *
   */
  const unsigned char *Data() const;

 private:
  circular_queue_a::Allocator<T, BUFFER_SIZE, MAX_PRODUCERS, MAX_CONSUMERS>
      allocator_;
};

// ----------------------------
// CircularQueueTypeA Implementation
// ----------------------------

template <class T, size_t BUFFER_SIZE, size_t MAX_PRODUCERS,
          size_t MAX_CONSUMERS>
typename CircularQueueTypeA<T, BUFFER_SIZE, MAX_PRODUCERS,
                            MAX_CONSUMERS>::Result
CircularQueueTypeA<T, BUFFER_SIZE, MAX_PRODUCERS, MAX_CONSUMERS>::Publish(
    const WriteSpan &span, size_t max_attempt) {
  // Attempt writing of data
  while (max_attempt) {
    // Allocate a write block on the buffer
    auto handle = allocator_.Allocate(span.Size(), max_attempt);
    if (handle) {
      // Write data
      memcpy(handle.Data(), span.Data(), span.Size() * sizeof(T));
      return Result::SUCCESS;
    }
    // Could not allocate chunk so attempt again
    --max_attempt;
  }
  return Result::ERROR_BUFFER_FULL;
}

template <class T, size_t BUFFER_SIZE, size_t MAX_PRODUCERS,
          size_t MAX_CONSUMERS>
typename CircularQueueTypeA<T, BUFFER_SIZE, MAX_PRODUCERS,
                            MAX_CONSUMERS>::Result
CircularQueueTypeA<T, BUFFER_SIZE, MAX_PRODUCERS, MAX_CONSUMERS>::Consume(
    ReadSpan &span, size_t max_attempt) const {
  // Attempt reading of data
  while (max_attempt) {
    // Allocate a read block on the buffer
    auto handle = allocator_.Allocate(max_attempt);
    if (handle) {
      span = std::move(handle);
      return Result::SUCCESS;
    }
    // Could not allocate chunk so attempt again
    --max_attempt;
  }
  return Result::ERROR_BUFFER_EMPTY;
}

template <class T, size_t BUFFER_SIZE, size_t MAX_PRODUCERS,
          size_t MAX_CONSUMERS>
const unsigned char *
CircularQueueTypeA<T, BUFFER_SIZE, MAX_PRODUCERS, MAX_CONSUMERS>::Data() const {
  return allocator_.Data();
}

// -------------------------

}  // namespace details
}  // namespace bigcat

#endif /* BIGCAT_MQ__DETAILS__CIRCULAR_QUEUE_A_HPP */
