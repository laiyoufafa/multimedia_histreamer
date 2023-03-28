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

#ifndef HISTREAMER_PIPELINE_FILTER_CODEC_BASE_H
#define HISTREAMER_PIPELINE_FILTER_CODEC_BASE_H

#include <string>
#include "foundation/osal/thread/task.h"
#include "foundation/utils/blocking_queue.h"
#include "foundation/utils/buffer_pool.h"
#include "pipeline/core/filter_base.h"
#include "pipeline/core/type_define.h"
#include "pipeline/filters/codec/codec_mode.h"
#include "pipeline/filters/common/plugin_utils.h"
#include "plugin/common/plugin_tags.h"
#include "plugin/core/codec.h"
#include "plugin/core/plugin_info.h"

namespace OHOS {
namespace Media {
namespace Pipeline {
class CodecFilterBase : public FilterBase, public Plugin::DataCallbackHelper {
public:
    explicit CodecFilterBase(const std::string &name);
    ~CodecFilterBase() override;

    ErrorCode Start() override;

    ErrorCode Stop() override;

    ErrorCode Prepare() override;

    ErrorCode SetParameter(int32_t key, const Plugin::Any& value) override;

    ErrorCode GetParameter(int32_t key, Plugin::Any& outVal) override;

    bool Negotiate(const std::string& inPort,
                   const std::shared_ptr<const Plugin::Capability>& upstreamCap,
                   Plugin::Capability& negotiatedCap,
                   const Plugin::TagMap& upstreamParams,
                   Plugin::TagMap& downstreamParams) override;

    bool Configure(const std::string &inPort, Plugin::TagMap &upstreamMeta,
                   Plugin::TagMap &upstreamParams, Plugin::TagMap &downstreamParams) override;

    void FlushStart() override;

    void FlushEnd() override;

protected:
    virtual uint32_t GetOutBufferPoolSize();

    virtual uint32_t CalculateBufferSize(Plugin::TagMap &meta);

    virtual Plugin::TagMap GetNegotiateParams(const Plugin::TagMap& upstreamParams);

    bool CheckRequiredOutCapKeys(const Capability& capability);

    virtual std::vector<Capability::Key> GetRequiredOutCapKeys();

    virtual ErrorCode ConfigureToStartPluginLocked(Plugin::TagMap &meta);

    ErrorCode UpdateMetaFromPlugin(Plugin::TagMap& meta);

    ErrorCode SetPluginParameterLocked(Tag tag, const Plugin::ValueType& value);

    virtual void UpdateParams(Plugin::TagMap &upMeta,
                              Plugin::TagMap &meta);

    template<typename T>
    ErrorCode GetPluginParameterLocked(Tag tag, T& value)
    {
        Plugin::Any tmp;
        auto err = TranslatePluginStatus(plugin_->GetParameter(tag, tmp));
        if (err == ErrorCode::SUCCESS && tmp.SameTypeWith(typeid(T))) {
            value = Plugin::AnyCast<T>(tmp);
        }
        return err;
    }

    bool isFlushing_ {false};
    Plugin::TagMap sinkParams_ {};
    std::shared_ptr<Plugin::Codec> plugin_ {};
    Plugin::BufferMetaType bufferMetaType_ = {};
    std::shared_ptr<CodecMode> codecMode_ {nullptr};
    Plugin::PluginType pluginType_ {};
    Plugin::CodecMode preferredCodecMode_ {Plugin::CodecMode::HARDWARE};
private:
    virtual std::shared_ptr<Allocator> GetAllocator();
    Capability capNegWithDownstream_ {};
};
} // namespace Pipeline
} // namespace Media
} // namespace OHOS
#endif // HISTREAMER_PIPELINE_FILTER_CODEC_BASE_H
