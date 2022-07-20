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

#include <bigcat_mq/details/shared_object.hpp>

#include "utils/threads.hpp"
#include "utils/chrono.hpp"

using namespace bigcat::details;

namespace {
constexpr auto kTestThreadCount = 10;
constexpr auto kTestCycleCount = 10;
constexpr auto kSharedObjectName = "/test_shared_object";

struct TestData {
  TestData() = default;
  TestData(const int value_) : value(value_) {}

  int value;
};

// Update value in the mock data type
__attribute__((no_sanitize("thread"))) void Update(TestData* obj) {
  utils::RandomDelayGenerator<> delay(10, 100);
  for (std::size_t i = 0; i < kTestCycleCount; ++i) {
    auto value = obj->value;
    std::this_thread::sleep_for(delay());
    obj->value = value + 1;
  }
}

}  // namespace

TEST(SharedObjectTestFixture, TestSingleThread) {
  auto obj = shared_object::GetOrCreate<TestData>(kSharedObjectName, 10);
  ASSERT_EQ(obj->value, 10);
  shared_object::Remove(kSharedObjectName);
}

TEST(SharedObjectTestFixture, TestMultipleThreads) {
  auto obj = shared_object::GetOrCreate<TestData>(kSharedObjectName, 0);
  utils::RunThreads(kTestThreadCount, &Update, obj);
  // NOTE: Due to the race condition the counter value should be less than max
  // possible value.
  ASSERT_LT(obj->value, kTestCycleCount * kTestThreadCount);
  shared_object::Remove(kSharedObjectName);
}