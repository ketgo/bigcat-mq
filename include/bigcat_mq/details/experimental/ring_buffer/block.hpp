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

#ifndef BIGCAT_MQ__DETAILS__EXPERIMENTAL__RING_BUFFER__BLOCK_HPP
#define BIGCAT_MQ__DETAILS__EXPERIMENTAL__RING_BUFFER__BLOCK_HPP

#include <type_traits>

#include <bigcat_mq/details/experimental/ring_buffer/cursor.hpp>

namespace bigcat {
namespace details {
namespace experimental {
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
 * @brief Handle to an allocated memory block in the ring buffer. It exposes
 * the allocated memory for reading or writing.
 *
 * @note The class does not satisfy CopyConstructable and CopyAssignable
 * concepts. However, it does satisfy MoveConstructable and MoveAssignable
 * concepts.
 *
 * @tparam T Type of objects stored in the memory block.
 * @tparam CursorPool The type of cursor pool.
 */
template <class T, class CursorPool>
class MemoryBlockHandle {
 public:
  /**
   * @brief Construct a new Memory Block Handle object.
   *
   */
  MemoryBlockHandle();

  /**
   * @brief Construct a new Memory Block Handle object.
   *
   * @param block Reference to the memory block.
   * @param handle Rvalue reference to the cursor handle.
   */
  MemoryBlockHandle(MemoryBlock<T>& block, CursorHandle<CursorPool>&& handle);

  /**
   * @brief Get the number of objects of type T stored in the memory block.
   *
   */
  std::size_t Size() const;

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
  T& operator[](std::size_t n) const;

  /**
   * @brief Check if the handle is valid.
   *
   */
  operator bool() const;

 private:
  MemoryBlock<T>* block_;
  CursorHandle<CursorPool> handle_;
};

// --------------------------------
// MemoryBlockHandle Implementation
// --------------------------------

template <class T, class CursorPool>
MemoryBlockHandle<T, CursorPool>::MemoryBlockHandle()
    : block_(nullptr), handle_() {}

template <class T, class CursorPool>
MemoryBlockHandle<T, CursorPool>::MemoryBlockHandle(
    MemoryBlock<T>& block, CursorHandle<CursorPool>&& handle)
    : block_(std::addressof(block)), handle_(handle) {}

template <class T, class CursorPool>
std::size_t MemoryBlockHandle<T, CursorPool>::Size() const {
  return block_->size;
}

template <class T, class CursorPool>
T* MemoryBlockHandle<T, CursorPool>::Data() const {
  return block_->data;
}

template <class T, class CursorPool>
T& MemoryBlockHandle<T, CursorPool>::operator[](std::size_t n) const {
  assert(n < block_->size);
  return block_->data[n];
}

template <class T, class CursorPool>
MemoryBlockHandle<T, CursorPool>::operator bool() const {
  return block_ != nullptr && bool(handle_);
}

// ============================================================================

}  // namespace ring_buffer
}  // namespace experimental
}  // namespace details
}  // namespace bigcat

#endif /* BIGCAT_MQ__DETAILS__EXPERIMENTAL__RING_BUFFER__BLOCK_HPP */
