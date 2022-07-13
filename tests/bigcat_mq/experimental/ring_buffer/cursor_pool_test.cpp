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

#include <bigcat_mq/details/experimental/ring_buffer/cursor.hpp>

#include "utils/random.hpp"
#include "utils/threads.hpp"

using namespace bigcat::details::experimental;

namespace {

constexpr auto kThreadCount = 10;
constexpr auto kPoolSize = 10;
constexpr auto kMaxAttempts = 32;
constexpr auto kBufferSize = 124;

using AtomicCursor = std::atomic<ring_buffer::Cursor>;
using CursorPool = ring_buffer::CursorPool<kPoolSize>;
using CursorHandle = ring_buffer::CursorHandle<CursorPool>;

// AtomicCursor hasher
class AtomicCursorHash {
 public:
  std::size_t operator()(const CursorHandle& handle) const {
    return hash_(&(*handle));
  }

 private:
  std::hash<AtomicCursor*> hash_;
};

// Allocate method run from different threads
void Allocate(CursorPool& pool, CursorHandle& handle) {
  handle = std::move(pool.Allocate(kMaxAttempts));
}

}  // namespace

TEST(RingBufferCursorPoolTestFixture, AllocateSingleThread) {
  CursorPool pool;

  std::unordered_set<CursorHandle, AtomicCursorHash> handles;
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

  std::array<CursorHandle, kThreadCount> handles;
  utils::Threads threads(kThreadCount);
  for (size_t i = 0; i < kThreadCount; ++i) {
    threads[i] = std::thread(&Allocate, std::ref(pool), std::ref(handles[i]));
  }
  threads.Wait();

  // Tracking null handles
  auto null_count = 0;
  std::unordered_set<CursorHandle, AtomicCursorHash> unique_handles;
  for (auto& handle : handles) {
    if (!handle) {
      ++null_count;
    }
    unique_handles.insert(std::move(handle));
  }
  ASSERT_EQ(unique_handles.size() - null_count, kThreadCount - null_count);
}

TEST(RingBufferCursorPoolTestFixture, IsBehindSingleThread) {
  utils::RandomNumberGenerator<size_t> rand(kBufferSize / 2,
                                            kBufferSize + kBufferSize / 2);
  size_t min = std::numeric_limits<size_t>::max();
  size_t max = std::numeric_limits<size_t>::min();
  std::array<CursorHandle, kThreadCount> handles;
  CursorPool pool;

  for (size_t i = 0; i < kThreadCount; ++i) {
    handles[i] = pool.Allocate(kMaxAttempts);
    if (handles[i]) {
      auto cursor = handles[i]->load(std::memory_order_seq_cst);
      cursor.SetLocation(rand());
      min = min > cursor.Location() ? cursor.Location() : min;
      max = max < cursor.Location() ? cursor.Location() : max;
      handles[i]->store(cursor, std::memory_order_seq_cst);
    }
  }
  auto cursor = pool.Head().load(std::memory_order_seq_cst);
  max += 10;
  cursor.SetLocation(max);
  pool.Head().store(cursor, std::memory_order_seq_cst);

  ASSERT_TRUE(pool.IsBehind({false, min / 2}));
  ASSERT_FALSE(pool.IsBehind({false, (min + max) / 2}));
  ASSERT_FALSE(pool.IsBehind({false, 2 * max}));
  ASSERT_FALSE(pool.IsBehind({true, min / 2}));
  ASSERT_TRUE(pool.IsBehind({true, 2 * max}));
}

TEST(RingBufferCursorPoolTestFixture, IsAheadOrEqualSingleThread) {
  utils::RandomNumberGenerator<size_t> rand(kBufferSize / 2,
                                            kBufferSize + kBufferSize / 2);
  size_t min = std::numeric_limits<size_t>::max();
  size_t max = std::numeric_limits<size_t>::min();
  std::array<CursorHandle, kThreadCount> handles;
  CursorPool pool;

  for (size_t i = 0; i < kThreadCount; ++i) {
    handles[i] = pool.Allocate(kMaxAttempts);
    if (handles[i]) {
      auto cursor = handles[i]->load(std::memory_order_seq_cst);
      cursor.SetLocation(rand());
      min = min > cursor.Location() ? cursor.Location() : min;
      max = max < cursor.Location() ? cursor.Location() : max;
      handles[i]->store(cursor, std::memory_order_seq_cst);
    }
  }
  auto cursor = pool.Head().load(std::memory_order_seq_cst);
  max += 10;
  cursor.SetLocation(max);
  pool.Head().store(cursor, std::memory_order_seq_cst);

  ASSERT_FALSE(pool.IsAheadOrEqual({false, min / 2}));
  ASSERT_FALSE(pool.IsAheadOrEqual({false, (min + max) / 2}));
  ASSERT_TRUE(pool.IsAheadOrEqual({false, max}));  // equality check
  ASSERT_TRUE(pool.IsAheadOrEqual({false, 2 * max}));
  ASSERT_TRUE(pool.IsAheadOrEqual({true, min / 2}));
  ASSERT_FALSE(pool.IsAheadOrEqual({true, 2 * max}));
}