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

#include <gtest/gtest.h>

#include <unordered_set>

#include "utils/debug.hpp"
#include "utils/random.hpp"
#include "utils/threads.hpp"

#include <bigcat_mq/details/experimental/ring_buffer/cursor.hpp>

using namespace bigcat::details::experimental;

namespace {

constexpr auto kThreadCount = 10;
constexpr auto kPoolSize = 10;
constexpr auto kMaxAttempts = 32;
constexpr auto kBufferSize = 124;

using CursorPool = ring_buffer::CursorPool<kPoolSize>;
using AtomicCursor = std::atomic<ring_buffer::Cursor>;

// AtomicCursor hasher
class AtomicCursorHash {
 public:
  std::size_t operator()(const CursorPool::CursorHandle& handle) const {
    return hash_(&(*handle));
  }

 private:
  std::hash<AtomicCursor*> hash_;
};

// Allocate method run from different threads
void Allocate(CursorPool& pool, CursorPool::CursorHandle& handle) {
  handle = std::move(pool.Allocate(kMaxAttempts));
}

}  // namespace

TEST(RingBufferCursorPoolTestFixture, AtomicCursorIsLockFree) {
  ASSERT_TRUE(AtomicCursor().is_lock_free());
}

TEST(RingBufferCursorPoolTestFixture, AllocateSingleThread) {
  CursorPool pool;

  std::unordered_set<CursorPool::CursorHandle, AtomicCursorHash> handles;
  for (size_t i = 0; i < kThreadCount; ++i) {
    auto handle = pool.Allocate(kMaxAttempts);
    if (handle) {
      ASSERT_TRUE(handles.find(handle) == handles.end());
      handles.insert(std::move(handle));
    }
  }
  ASSERT_NE(handles.size(), 0);
}

TEST(RingBufferCursorPoolTestFixture, AllocateMultipleThread) {
  CursorPool pool;

  std::array<CursorPool::CursorHandle, kThreadCount> handles;
  utils::Threads threads(kThreadCount);
  for (size_t i = 0; i < kThreadCount; ++i) {
    threads[i] = std::thread(&Allocate, std::ref(pool), std::ref(handles[i]));
  }
  threads.Wait();

  // Tracking null handles
  auto null_count = 0;
  std::unordered_set<CursorPool::CursorHandle, AtomicCursorHash> unique_handles;
  for (auto& handle : handles) {
    if (!handle) {
      ++null_count;
    }
    unique_handles.insert(std::move(handle));
  }
  ASSERT_EQ(unique_handles.size() - null_count, kThreadCount - null_count);
}
