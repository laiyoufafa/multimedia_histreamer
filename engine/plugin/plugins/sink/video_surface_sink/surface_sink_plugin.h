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

#ifndef HISTREAMER_SURFACE_SINK_PLUGIN_H
#define HISTREAMER_SURFACE_SINK_PLUGIN_H

#if !defined(OHOS_LITE) && defined(VIDEO_SUPPORT)

#include <atomic>
#include <memory>
#include "refbase.h"
#include "surface/surface.h"
#include "display_type.h"
#include "common/graphic_common.h"
#include "plugin/common/surface_allocator.h"
#include "plugin/common/plugin_video_tags.h"
#include "plugin/interface/video_sink_plugin.h"

#ifdef DUMP_RAW_DATA
#include <fstream>
#endif

namespace OHOS {
namespace Media {
namespace Plugin {
namespace SurfaceSink {
class SurfaceSinkPlugin : public VideoSinkPlugin, public std::enable_shared_from_this<SurfaceSinkPlugin> {
public:
    explicit SurfaceSinkPlugin(std::string name);
    ~SurfaceSinkPlugin() override = default;

    Status Init() override;

    Status Deinit() override;

    Status Prepare() override;

    Status Reset() override;

    Status Start() override;

    Status Stop() override;

    Status GetParameter(Tag tag, ValueType &value) override;

    Status SetParameter(Tag tag, const ValueType &value) override;

    std::shared_ptr<Allocator> GetAllocator() override;

    Status SetCallback(Callback* cb) override;

    Status Pause() override;

    Status Resume() override;

    Status Write(const std::shared_ptr<Buffer> &input) override;

    Status Flush() override;

    Status GetLatency(uint64_t &nanoSec) override;

private:
    uint32_t width_;
    uint32_t height_;
    uint32_t stride_;
    VideoPixelFormat pixelFormat_;
    sptr<Surface> surface_ {nullptr};
    std::shared_ptr<SurfaceAllocator> mAllocator_ {nullptr};
    uint32_t maxSurfaceNum_;

#ifdef DUMP_RAW_DATA
    std::ofstream dumpData_;
#endif
};
} // SurfaceSink
} // Plugin
} // Media
} // OHOS
#endif
#endif // MEDIA_PIPELINE_SDL_VIDEO_SINK_PLUGIN_H
