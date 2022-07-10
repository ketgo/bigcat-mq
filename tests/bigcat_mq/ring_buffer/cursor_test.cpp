// Copyright 2022 Ketan Goyal
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>

#include "utils/chrono.hpp"
#include "utils/debug.hpp"
#include "utils/threads.hpp"

#include <bigcat_mq/details/ring_buffer/cursor.hpp>

namespace {
constexpr auto kThreadCount = 10;
constexpr auto kPoolSize = 10;

using CursorPool = bigcat::details::ring_buffer::CursorPool<kPoolSize>;
}  // namespace

TEST(RingBufferCursorPoolTestFixture, AllocateReleaseSingleThread) {}