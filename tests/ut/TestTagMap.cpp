/*
 * Copyright (c) 2022-2022 Huawei Device Co., Ltd.
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

#include <cstdlib>
#include <memory>
#include <string>
#include "gtest/gtest.h"
#define private public
#define protected public
#include "plugin/common/plugin_meta.h"

namespace OHOS {
namespace Media {
namespace Test {
using namespace OHOS::Media::Plugin;

class TestMeta : public ::testing::Test {
public:
    void SetUp() override
    {
    }

    void TearDown() override
    {
    }
};

TEST_F(TestMeta, Can_insert_tag_value_int64_to_Meta)
{
    Meta map;
    ASSERT_TRUE(map.Set<Tag::MEDIA_DURATION>(10000));
    ASSERT_TRUE(map.Set<Tag::MEDIA_FILE_SIZE>(500));
    int64_t value;
    ASSERT_TRUE(map.Get<Tag::MEDIA_DURATION>(value));
    ASSERT_EQ(10000, value);
    uint64_t size;
    ASSERT_TRUE(map.Get<Tag::MEDIA_FILE_SIZE>(size));
    ASSERT_EQ(500, value);
}

TEST_F(TestMeta, Can_insert_tag_value_uint32_to_TagMap)
{
    Meta map;
    ASSERT_TRUE(map.Set<Tag::TRACK_ID>(10000));
    uint32_t value;
    ASSERT_TRUE(map.Get<Tag::TRACK_ID>(value));
    ASSERT_EQ(10000, value);
}
} // namespace Test
} // namespace Media
} // namespace OHOS