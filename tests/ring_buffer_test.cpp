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

#include <string>
#include <vector>

#include <bigcat_mq/details/ring_buffer.hpp>

TEST(RingBufferDetailsTestFixture, TestRand) {
  ASSERT_NE(bigcat::details::ring_buffer::rand(),
            bigcat::details::ring_buffer::rand());
}

class RingBufferTestFixture : public ::testing::Test {
 protected:
  using RingBuffer = bigcat::details::RingBuffer<char, 100, 5, 5>;
  RingBuffer buffer_;

  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(RingBufferTestFixture, TestPublishConsumeSingleThread) {
  std::string write_data = "testing";
  RingBuffer::ReadSpan span;

  ASSERT_EQ(buffer_.Publish(write_data),
            bigcat::details::RingBufferResult::SUCCESS);
  ASSERT_EQ(buffer_.Consume(span), bigcat::details::RingBufferResult::SUCCESS);
  ASSERT_EQ(write_data, std::string(span.Data(), span.Size()));
}
