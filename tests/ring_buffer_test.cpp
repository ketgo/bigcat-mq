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
  bigcat::details::RingBuffer<char> buffer_ = {100, 5, 5};

  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(RingBufferTestFixture, TestPublishConsumeSingleThread) {
  std::string write_data = "testing";
  std::vector<char> read_data(write_data.size());
  bigcat::details::ConstSpan<char> span;

  ASSERT_EQ(buffer_.Publish(write_data.c_str(), write_data.size()),
            bigcat::details::RingBufferResult::SUCCESS);
  ASSERT_EQ(buffer_.Consume(span), bigcat::details::RingBufferResult::SUCCESS);
  memcpy(read_data.data(), span.Data(), read_data.size());
  ASSERT_EQ(write_data, std::string(read_data.begin(), read_data.end()));
}
