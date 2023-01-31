/*
 * Copyright (c) 2023-2023 Huawei Device Co., Ltd.
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

#if !defined(OHOS_LITE) && defined(VIDEO_SUPPORT)

#ifndef HISTREAMER_PLUGIN_CODEC_BUFFER_H
#define HISTREAMER_PLUGIN_CODEC_BUFFER_H

#include "common/plugin_buffer.h"
#include "common/share_memory.h"

namespace OHOS {
namespace Media {
namespace Plugin {
namespace CodecAdapter {
class CodecBuffer {
public:
    explicit CodecBuffer(std::shared_ptr<Buffer>);
    ~CodecBuffer() = default;

private:
    Status InitBuffer(uint32_t CodecBufferType);

    std::shared_ptr<ShareMemory> sharedMem_;
    BufferHandle* bufferHandle_;
};
} // namespace CodecAdapter
} // namespace Plugin
} // namespace Media
} // namespace OHOS
#endif // HISTREAMER_PLUGIN_CODEC_BUFFER_H
#endif