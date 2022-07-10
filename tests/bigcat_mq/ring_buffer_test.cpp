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

#include <unordered_set>

#include <gtest/gtest.h>

#include <bigcat_mq/details/ring_buffer.hpp>

#include "utils/chrono.hpp"
#include "utils/debug.hpp"
#include "utils/threads.hpp"

TEST(RingBufferDetailsTestFixture, TestRand) {
  ASSERT_NE(bigcat::details::ring_buffer::rand(),
            bigcat::details::ring_buffer::rand());
}

class RingBufferTestFixture : public ::testing::Test {
 protected:
  static constexpr auto kBufferSize = 100;
  static constexpr auto kMaxProducers = 5;
  static constexpr auto kMaxConsumers = 5;

  using RingBuffer = bigcat::details::RingBuffer<char, kBufferSize,
                                                 kMaxProducers, kMaxConsumers>;
  RingBuffer buffer_;

 public:
  // Publish data to ring buffer
  void Publish(const std::string& data,
               std::chrono::microseconds delay = std::chrono::microseconds{0}) {
    do {
      std::this_thread::sleep_for(delay);
    } while (buffer_.Publish(data) !=
             bigcat::details::RingBufferResult::SUCCESS);
  }

  // Consume data from ring buffer
  void Consume(std::string& data,
               std::chrono::microseconds delay = std::chrono::microseconds{
                   0}) const {
    RingBuffer::ReadSpan span;

    do {
      std::this_thread::sleep_for(delay);
    } while (buffer_.Consume(span) !=
             bigcat::details::RingBufferResult::SUCCESS);

    data = std::string(span.Data(), span.Size());
    DPRINT(span.Data());
    DPRINT(span.Size());
  }
};

template <class T, class U>
void Print(const U* start, const size_t size, const char sep = ',',
           const char end = '\n') {
  const auto len = size * sizeof(U) / sizeof(T);
  for (size_t i = 0; i < len; ++i) {
    std::cout << *(reinterpret_cast<const T*>(start) + i) << sep;
  }
  std::cout << end;
}

TEST_F(RingBufferTestFixture, TestPublishConsumeSingleThread) {
  constexpr auto kMessageCount = 2;

  std::unordered_set<std::string> write_data;
  for (size_t i = 0; i < kMessageCount; ++i) {
    std::string data = "testing_" + std::to_string(i);
    ASSERT_EQ(buffer_.Publish(data),
              bigcat::details::RingBufferResult::SUCCESS);
    write_data.insert(data);
  }

  Print<unsigned char>(buffer_.Data(), kBufferSize);

  for (size_t i = 0; i < kMessageCount; ++i) {
    RingBuffer::ReadSpan span;
    ASSERT_EQ(buffer_.Consume(span),
              bigcat::details::RingBufferResult::SUCCESS);
    std::string data(span.Data(), span.Size());
    ASSERT_TRUE(write_data.find(data) != write_data.end());
  }
}

/*
TEST_F(RingBufferTestFixture, TestPublishMultipleThreadsConsumeSingleThread) {
  constexpr auto kThreadCount = 5;

  std::array<std::string, kThreadCount> write_data;
  std::array<std::string, kThreadCount> read_data;
  for (size_t i = 0; i < kThreadCount; ++i) {
    write_data[i] = "testing_" + std::to_string(i);
  }
  utils::Threads producers(kThreadCount);
  utils::RandomDelayGenerator<> rand(1, 5);

  for (size_t i = 0; i < kThreadCount; ++i) {
    producers[i] = std::thread(&RingBufferTestFixture::Publish, this,
                               std::ref(write_data[i]), rand());
  }
  producers.Wait();

  Print<unsigned char>(buffer_.Data(), kBufferSize);

  for (size_t i = 0; i < kThreadCount; ++i) {
    Consume(read_data[i]);
    std::cout << read_data[i] << "\n";
  }

  ASSERT_FALSE(true);
}

TEST_F(RingBufferTestFixture, TestPublishConsumeMultipleThreads) {
  constexpr auto kProducers = 5;
  constexpr auto kConsumers = 5;

  std::array<std::string, kProducers> write_data;
  std::array<std::string, kConsumers> read_data;
  for (size_t i = 0; i < kProducers; ++i) {
    write_data[i] = "testing_" + std::to_string(i);
  }
  utils::Threads producers(kProducers);
  utils::Threads consumers(kConsumers);
  utils::RandomDelayGenerator<> rand(1, 5);

  DPRINT("PRODUCERS");
  for (size_t i = 0; i < kProducers; ++i) {
    producers[i] = std::thread(&RingBufferTestFixture::Publish, this,
                               std::ref(write_data[i]), rand());
  }
  producers.Wait();

  DPRINT("CONSUMERS");
  for (size_t i = 0; i < kConsumers; ++i) {
    consumers[i] = std::thread(&RingBufferTestFixture::Consume, this,
                               std::ref(read_data[i]), rand());
  }
  consumers.Wait();

  Print<unsigned char>(buffer_.Data(), kBufferSize);

    ASSERT_EQ(write_data, read_data);
}
*/