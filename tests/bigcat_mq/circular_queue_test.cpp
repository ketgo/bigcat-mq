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

#include <algorithm>
#include <unordered_set>

#include <gtest/gtest.h>

#include <bigcat_mq/details/circular_queue.hpp>

#include "utils/chrono.hpp"
#include "utils/threads.hpp"

using namespace bigcat::details;

class CircularQueueTestFixture : public ::testing::Test {
 protected:
  static constexpr auto kBufferSize = 521;
  static constexpr auto kMaxProducers = 100;
  static constexpr auto kMaxConsumers = 100;

  using Queue = CircularQueue<char, kBufferSize, kMaxProducers, kMaxConsumers>;
  Queue queue_;

 public:
  // Publish data to ring buffer
  void Publish(const std::string& data,
               std::chrono::microseconds delay = std::chrono::microseconds{0}) {
    while (true) {
      std::this_thread::sleep_for(delay);
      if (queue_.Publish(data) == CircularQueueResult::SUCCESS) {
        break;
      }
      std::this_thread::yield();
    }
  }

  // Consume data from ring buffer
  void Consume(std::string& data,
               std::chrono::microseconds delay = std::chrono::microseconds{
                   0}) const {
    Queue::ReadSpan span;
    while (true) {
      std::this_thread::sleep_for(delay);
      if (queue_.Consume(span) == CircularQueueResult::SUCCESS) {
        break;
      }
      std::this_thread::yield();
    }
    data = std::string(span.Data(), span.Size());
  }
};

TEST_F(CircularQueueTestFixture, TestPublishConsumeSingleThread) {
  constexpr auto kMessageCount = 10;

  std::unordered_set<std::string> write_data;
  for (size_t i = 0; i < kMessageCount; ++i) {
    std::string data = "testing_" + std::to_string(i);
    ASSERT_EQ(queue_.Publish(data), CircularQueueResult::SUCCESS);
    write_data.insert(data);
  }

  for (size_t i = 0; i < kMessageCount; ++i) {
    Queue::ReadSpan span;
    ASSERT_EQ(queue_.Consume(span), CircularQueueResult::SUCCESS);
    std::string data(span.Data(), span.Size());
    ASSERT_TRUE(write_data.find(data) != write_data.end());
  }
}

TEST_F(CircularQueueTestFixture, TestPublishConsumeMultipleThreadsSerial) {
  constexpr auto kProducers = 10;
  constexpr auto kConsumers = 10;

  std::array<std::string, kProducers> write_data;
  std::array<std::string, kConsumers> read_data;
  for (size_t i = 0; i < kProducers; ++i) {
    write_data[i] = "testing_" + std::to_string(i);
  }
  utils::RandomDelayGenerator<> rand(1, 5);

  {
    utils::Threads producers(kProducers);
    for (size_t i = 0; i < kProducers; ++i) {
      producers[i] = std::thread(&CircularQueueTestFixture::Publish, this,
                                 std::ref(write_data[i]), rand());
    }
  }

  {
    utils::Threads consumers(kConsumers);
    for (size_t i = 0; i < kConsumers; ++i) {
      consumers[i] = std::thread(&CircularQueueTestFixture::Consume, this,
                                 std::ref(read_data[i]), rand());
    }
  }

  for (auto& data : write_data) {
    ASSERT_TRUE(std::find(read_data.begin(), read_data.end(), data) !=
                read_data.end());
  }
}

TEST_F(CircularQueueTestFixture, TestPublishConsumeMultipleThreadsConcurrent) {
  constexpr auto kProducers = 10;
  constexpr auto kConsumers = 10;

  std::array<std::string, kProducers> write_data;
  std::array<std::string, kConsumers> read_data;
  for (size_t i = 0; i < kProducers; ++i) {
    write_data[i] = "testing_" + std::to_string(i);
  }
  utils::RandomDelayGenerator<> rand(1, 5);

  {
    utils::Threads producers(kProducers);
    utils::Threads consumers(kConsumers);
    for (size_t i = 0; i < kProducers; ++i) {
      producers[i] = std::thread(&CircularQueueTestFixture::Publish, this,
                                 std::ref(write_data[i]), rand());
    }
    for (size_t i = 0; i < kConsumers; ++i) {
      consumers[i] = std::thread(&CircularQueueTestFixture::Consume, this,
                                 std::ref(read_data[i]), rand());
    }
  }

  for (auto& data : write_data) {
    ASSERT_TRUE(std::find(read_data.begin(), read_data.end(), data) !=
                read_data.end());
  }
}
