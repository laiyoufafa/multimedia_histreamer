/*
 * Copyright (c) 2021-2021 Huawei Device Co., Ltd.
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
#ifndef OHOS_LITE
#include <utility>
#include "surface_buffer.h"

namespace OHOS {
namespace Media {
namespace Plugin {
SurfaceMemory::SurfaceMemory(size_t capacity, std::shared_ptr<Allocator> allocator, size_t align)
    : Memory(capacity, std::move(allocator), align, MemoryType::SURFACE_BUFFER)
{
}

sptr<SurfaceBuffer> SurfaceMemory::GetSurfaceBuffer()
{
    return surfaceBuffer;
}

int32_t SurfaceMemory::GetFenceFd()
{
    return fenceFd;
}

uint8_t* SurfaceMemory::GetRealAddr() const
{
    return static_cast<uint8_t*>(surfaceBuffer->GetVirAddr());
}
} // namespace Plugin
} // namespace Media
} // namespace OHOS
#endif