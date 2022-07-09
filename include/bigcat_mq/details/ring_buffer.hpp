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

/**
 * Ring Buffer
 * ===========
 *
 * The ring buffer is designed to be lock-free and wait-free supporting multiple
 * consumers and producers. It exposes two operations, `publish` for writing and
 * `consume` for reading. Details on the two are discussed in the following
 * sections.
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
 * Data is written on the ring buffer in the form of `MemoryBlock` data
 * structure. The data structure contains two parts: header and body. The header
 * contains the size of the body and possibly a checksum. The body contains the
 * data published by a producer.
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
 */

#ifndef BIGCAT_MQ__DETAILS__RING_BUFFER_HPP
#define BIGCAT_MQ__DETAILS__RING_BUFFER_HPP

#include <array>
#include <string>
#include <type_traits>
#include <vector>

#include <bigcat_mq/details/ring_buffer/allocator.hpp>

namespace bigcat {
namespace details {

// ============================================================================

/**
 * @brief Enumerated set of results returned by the RingBuffer.
 *
 */
enum class RingBufferResult {
  SUCCESS = 0,             // Successful completion of operation
  ERROR_BUFFER_FULL = 1,   // Buffer full error
  ERROR_BUFFER_EMPTY = 2,  // Buffer empty error
};

// ============================================================================

/**
 * @brief Span encapsulating data ready to be written into the ring buffer.
 *
 * @tparam T Type of object stored.
 */
template <class T>
class WriteSpan {
 public:
  /**
   * @brief Construct a new WriteSpan object.
   *
   */
  WriteSpan() : size_(0), data_(nullptr) {}

  /**
   * @brief Construct a new WriteSpan object
   *
   * @param data Constant pointer to the data.
   * @param size Size of the data.
   */
  WriteSpan(const T *data, const size_t size) : size_(size), data_(data) {}

  /**
   * @brief Construct a new WriteSpan object.
   *
   * @tparam N Size of array.
   */
  template <size_t N>
  WriteSpan(T (&array)[N]) : size_(N), data_(array) {}

  /**
   * @brief Construct a new WriteSpan object
   *
   * @tparam N Size of array.
   * @param array Constant reference to the array.
   */
  template <std::size_t N>
  WriteSpan(const std::array<T, N> &array)
      : size_(array.size()), data_(array.data()) {}

  /**
   * @brief Construct a new WriteSpan object.
   *
   * @param vector Constant reference to the vector.
   */
  WriteSpan(const std::vector<T> &vector)
      : size_(vector.size()), data_(vector.data()) {}

  /**
   * @brief Construct a new WriteSpan object.
   *
   * @param string Constant reference to the string.
   */
  template <class U = T,
            typename std::enable_if<std::is_same<U, char>::value>::type...>
  WriteSpan(const std::string &string)
      : size_(string.size()), data_(string.data()) {}

  /**
   * @brief Get the number of objects of type T stored in the span.
   *
   */
  size_t Size() const;

  /**
   * @brief Get the pointer to the first T type object in the span.
   *
   */
  const T *Data() const;

 private:
  size_t size_;
  const T *data_;
};

// -------------------------
// WriteSpan Implementation
// -------------------------

template <class T>
size_t WriteSpan<T>::Size() const {
  return size_;
}

template <class T>
const T *WriteSpan<T>::Data() const {
  return data_;
}

// ============================================================================

/**
 * @brief Span encapsulating data in the ring buffer ready to be read.
 *
 * TODO: Use composition to handle the need of cursor pool type template
 * parameter
 *
 * @tparam T Type of object stored.
 */
template <class T>
class ReadSpan {
 public:
  template <class CursorPool>

  /**
   * @brief Get the number of objects of type T stored in the span.
   *
   */
  size_t Size() const;

  /**
   * @brief Get the pointer to the first T type object in the span.
   *
   */
  const T *Data() const;

 private:
  details::ring_buffer::MemoryBlockHandle<const T>;
};

// ============================================================================

/**
 * @brief A lock-free and wait-free ring buffer supporting multiple consumers
 * and producers.
 *
 * TODO: Release cursors for stale processes which have died abruptly.
 *
 * @tparam T The type of object stored in ring buffer.
 * @tparam BUFFER_SIZE The size of memory buffer in bytes.
 * @tparam MAX_PRODUCERS The maximum number of producers.
 * @tparam MAX_CONSUMERS The maximum number of consumers.
 */
template <class T, size_t BUFFER_SIZE, size_t MAX_PRODUCERS,
          size_t MAX_CONSUMERS>
class RingBuffer {
  // Ensures at compile time the parameter T has trivial memory layout.
  static_assert(std::is_trivial<T>::value,
                "The data type used does not have a trivial memory layout.");

 public:
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
  RingBufferResult Publish(const WriteSpan<T> &span,
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
  RingBufferResult Consume(ReadSpan<T> &span,
                           size_t max_attempt = defaultMaxAttempt()) const;

 private:
  details::ring_buffer::Allocator<T, BUFFER_SIZE, MAX_PRODUCERS, MAX_CONSUMERS>
      allocator_;
};

// -------------------------
// RingBuffer Implementation
// -------------------------

template <class T, size_t BUFFER_SIZE, size_t MAX_PRODUCERS,
          size_t MAX_CONSUMERS>
RingBufferResult RingBuffer<T, BUFFER_SIZE, MAX_PRODUCERS,
                            MAX_CONSUMERS>::Publish(const WriteSpan<T> &span,
                                                    size_t max_attempt) {
  // Attempt writing of data
  while (max_attempt) {
    // Allocate a write block on the buffer
    auto handle = allocator_.Allocate(span.Size(), max_attempt);
    if (handle) {
      // Write data
      memcpy(handle.Data(), span.Data(), span.Size() * sizeof(T));
      return RingBufferResult::SUCCESS;
    }
    // Could not allocate chunk so attempt again
    --max_attempt;
  }
  return RingBufferResult::ERROR_BUFFER_FULL;
}

template <class T, size_t BUFFER_SIZE, size_t MAX_PRODUCERS,
          size_t MAX_CONSUMERS>
RingBufferResult RingBuffer<T, BUFFER_SIZE, MAX_PRODUCERS,
                            MAX_CONSUMERS>::Consume(ReadSpan<T> &span,
                                                    size_t max_attempt) const {
  // Attempt reading of data
  while (max_attempt) {
    // Allocate a read block on the buffer
    auto handle = allocator_.Allocate(max_attempt);
    if (handle) {
      span = std::move(handle);
      return RingBufferResult::SUCCESS;
    }
    // Could not allocate chunk so attempt again
    --max_attempt;
  }
  return RingBufferResult::ERROR_BUFFER_EMPTY;
}

// ============================================================================

}  // namespace details
}  // namespace bigcat

#endif /* BIGCAT_MQ__DETAILS__RING_BUFFER_HPP */
