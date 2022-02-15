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

#ifndef HISTREAMER_PIPELINE_FILTER_AUDIO_DECODER_H
#define HISTREAMER_PIPELINE_FILTER_AUDIO_DECODER_H

#include "filters/codec/codec_filter_base.h"

namespace OHOS {
namespace Media {
namespace Pipeline {
class AudioDecoderFilter : public CodecFilterBase {
public:
    explicit AudioDecoderFilter(const std::string &name);
    ~AudioDecoderFilter() override;

    ErrorCode Start() override;

    ErrorCode Stop() override;

    bool Negotiate(const std::string& inPort,
                   const std::shared_ptr<const Plugin::Capability>& upstreamCap,
                   Plugin::Capability& negotiatedCap,
                   const Plugin::TagMap& upstreamParams,
                   Plugin::TagMap& downstreamParams) override;

    bool Configure(const std::string& inPort, const std::shared_ptr<const Plugin::Meta>& upstreamMeta) override;

    /**
     *
     * @param inPort
     * @param buffer
     * @param offset always ignore this parameter
     * @return
     */
    ErrorCode PushData(const std::string &inPort, AVBufferPtr buffer, int64_t offset) override;

    void FlushStart() override;

    void FlushEnd() override;

private:
    ErrorCode ConfigureToStartPluginLocked(const std::shared_ptr<const Plugin::Meta> &meta);

    ErrorCode HandleFrame(const std::shared_ptr<AVBuffer>& buffer);

    ErrorCode FinishFrame();

    ErrorCode Release();

    void OnInputBufferDone(const std::shared_ptr<Plugin::Buffer>& input) override;
    void OnOutputBufferDone(const std::shared_ptr<Plugin::Buffer>& output) override;
private:
    std::shared_ptr<BufferPool<AVBuffer>> outBufferPool_ {};
    bool isFlushing_ {false};

    Capability capNegWithDownstream_;
    Capability capNegWithUpstream_;
};
} // Pipeline
} // Media
} // OHOS
#endif // HISTREAMER_PIPELINE_FILTER_AUDIO_DECODER_H
