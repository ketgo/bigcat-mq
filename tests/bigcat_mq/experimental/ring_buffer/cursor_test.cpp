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

#include <bigcat_mq/details/experimental/ring_buffer/cursor.hpp>

using namespace bigcat::details::experimental;

namespace {

using Cursor = ring_buffer::Cursor;
using AtomicCursor = std::atomic<ring_buffer::Cursor>;

}  // namespace

TEST(RingBufferCursorTestFixture, AddOperation) {
  Cursor cursor(false, std::numeric_limits<std::size_t>::max());

  auto new_cursor = cursor + 5;
  ASSERT_TRUE(new_cursor.Overflow());
  ASSERT_EQ(new_cursor.Location(), 4);
}

TEST(RingBufferCursorTestFixture, AtomicCursorIsLockFree) {
  ASSERT_TRUE(AtomicCursor().is_lock_free());
}
