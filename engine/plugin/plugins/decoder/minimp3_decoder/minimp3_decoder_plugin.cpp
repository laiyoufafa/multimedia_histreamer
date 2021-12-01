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

#define HST_LOG_TAG "Minimp3DecoderPlugin"

#include "minimp3_decoder_plugin.h"
#include <cstring>
#include <map>
#include <set>
#include "utils/constants.h"
#include "utils/memory_helper.h"

#include "plugin/common/plugin_audio_tags.h"
#include "plugin/common/plugin_buffer.h"
#include "plugin/interface/codec_plugin.h"

namespace OHOS {
namespace Media {
namespace Plugin {
namespace {
    constexpr uint32_t MP3_384_SAMPLES_PER_FRAME  = 384;
    constexpr uint32_t MP3_576_SAMPLES_PER_FRAME  = 576;
    constexpr uint32_t MP3_1152_SAMPLES_PER_FRAME = 1152;
    constexpr uint32_t BUFFER_ITEM_CNT            = 6;
    constexpr uint32_t MAX_RANK                   = 100;
    Status RegisterDecoderPlugin(const std::shared_ptr<Register>& reg);
    void UpdatePluginDefinition(CodecPluginDef& definition);
}

Minimp3DecoderPlugin::Minimp3DecoderPlugin(std::string name)
    : CodecPlugin(std::move(name)),
      mp3Parameter_()
{
    (void)memset_s(&mp3DecoderAttr_, sizeof(mp3DecoderAttr_), 0x00, sizeof(AudioDecoderMp3Attr));
    MEDIA_LOG_I("Minimp3DecoderPlugin, plugin name: %s", pluginName_.c_str());
}

Minimp3DecoderPlugin::~Minimp3DecoderPlugin()
{
    MEDIA_LOG_I("~Minimp3DecoderPlugin, plugin name: %s", pluginName_.c_str());
}

Status Minimp3DecoderPlugin::Init()
{
    minimp3DecoderImpl_ = MiniMp3GetOpt();
    AudioDecoderMp3Open();
    mp3Parameter_[Tag::REQUIRED_OUT_BUFFER_CNT] = BUFFER_ITEM_CNT;
    return Status::OK;
}

Status Minimp3DecoderPlugin::Deinit()
{
    AudioDecoderMp3Close();
    return Status::OK;
}

Status Minimp3DecoderPlugin::SetParameter(Tag tag, const ValueType& value)
{
    mp3Parameter_.insert(std::make_pair(tag, value));
    return Status::OK;
}

Status Minimp3DecoderPlugin::GetParameter(Tag tag, ValueType& value)
{
    auto res = mp3Parameter_.find(tag);
    if (res != mp3Parameter_.end()) {
        value = res->second;
        return Status::OK;
    }
    return Status::ERROR_INVALID_PARAMETER;
}

Status Minimp3DecoderPlugin::Prepare()
{
    if (mp3Parameter_.find(Tag::AUDIO_CHANNELS) != mp3Parameter_.end()) {
        channels_ = AnyCast<uint32_t>((mp3Parameter_.find(Tag::AUDIO_CHANNELS))->second);
    }

    if (mp3Parameter_.find(Tag::AUDIO_SAMPLE_PER_FRAME) != mp3Parameter_.end()) {
        samplesPerFrame_ = AnyCast<uint32_t>(mp3Parameter_.find(Tag::AUDIO_SAMPLE_PER_FRAME)->second);
    }

    if (samplesPerFrame_ != MP3_384_SAMPLES_PER_FRAME && samplesPerFrame_ != MP3_576_SAMPLES_PER_FRAME && samplesPerFrame_ != MP3_1152_SAMPLES_PER_FRAME) {
        return Status::ERROR_INVALID_PARAMETER;
    }

    MEDIA_LOG_I("channels_ = %d samplesPerFrame_ = %d", channels_, samplesPerFrame_);

    return Status::OK;
}

Status Minimp3DecoderPlugin::Reset()
{
    mp3Parameter_.clear();
    return Status::OK;
}

Status Minimp3DecoderPlugin::Start()
{
    return Status::OK;
}

Status Minimp3DecoderPlugin::Stop()
{
    return Status::OK;
}

bool Minimp3DecoderPlugin::IsParameterSupported(Tag tag) 
{
    (void)tag;
    return false;
}

std::shared_ptr<Allocator> Minimp3DecoderPlugin::GetAllocator()
{
    return nullptr;
}

Status Minimp3DecoderPlugin::SetCallback(const std::shared_ptr<Callback>& cb)
{
    return Status::OK;
}

Status Minimp3DecoderPlugin::GetPcmDataProcess(const std::shared_ptr<Buffer>& inputBuffer, std::shared_ptr<Buffer>& outputBuffer)
{
    if (inputBuffer == nullptr) {
        return Status::ERROR_NOT_ENOUGH_DATA;
    }
    if (inputBuffer->IsEmpty() && (inputBuffer->flag & BUFFER_FLAG_EOS) != 0) {
        MEDIA_LOG_I("eos received");
        outputBuffer->GetMemory()->Reset();
        outputBuffer->flag |= BUFFER_FLAG_EOS;
        return Status::END_OF_STREAM;
    } else if (outputBuffer == nullptr || outputBuffer->IsEmpty()) {
        MEDIA_LOG_W("outputBuffer nullptr warning");
        return Status::ERROR_INVALID_PARAMETER;
    }

    return AudioDecoderMp3Process(inputBuffer_, outputBuffer);
}

Status Minimp3DecoderPlugin::QueueOutputBuffer(const std::shared_ptr<Buffer>& outputBuffers, int32_t timeoutMs)
{
    MEDIA_LOG_D("queue output buffer");
    (void)timeoutMs;
    if (!outputBuffers) {
        return Status::ERROR_INVALID_PARAMETER;
    }
    outputBuffer_ = outputBuffers;
    return Status::OK;
}

Status Minimp3DecoderPlugin::DequeueOutputBuffer(std::shared_ptr<Buffer>& outputBuffers, int32_t timeoutMs)
{
    MEDIA_LOG_D("dequeue output buffer");
    (void)timeoutMs;
    OSAL::ScopedLock lock(ioMutex_);
    Status status = GetPcmDataProcess(inputBuffer_, outputBuffer_);
    inputBuffer_.reset();
    inputBuffer_ = nullptr;
    outputBuffers.reset();
    if (status == Status::OK || status == Status::END_OF_STREAM) {
        outputBuffers = outputBuffer_;
    }
    outputBuffer_.reset();
    outputBuffer_ = nullptr;
    return status;
}

Status Minimp3DecoderPlugin::QueueInputBuffer(const std::shared_ptr<Buffer>& inputBuffer, int32_t timeoutMs)
{
    MEDIA_LOG_D("queue input buffer");
    (void)timeoutMs;
    if (inputBuffer->IsEmpty() && !(inputBuffer->flag & BUFFER_FLAG_EOS)) {
        MEDIA_LOG_E("decoder does not support fd buffer");
        return Status::ERROR_INVALID_DATA;
    } else {
        inputBuffer_ = inputBuffer;
        return Status::OK;
    }
}

Status Minimp3DecoderPlugin::DequeueInputBuffer(std::shared_ptr<Buffer>& inputBuffer, int32_t timeoutMs)
{
    MEDIA_LOG_D("dequeue input buffer");
    (void)timeoutMs;
    inputBuffer = inputBuffer_;
    return Status::OK;
}

Status Minimp3DecoderPlugin::Flush()
{
    return Status::OK;
}

Status Minimp3DecoderPlugin::SetDataCallback(const std::weak_ptr<DataCallback>& dataCallback) 
{
    dataCb_ = dataCallback;
    return Status::OK;
}

void Minimp3DecoderPlugin::AudioDecoderMp3Open()
{
    Minimp3WrapperMp3decInit(&mp3DecoderAttr_.mp3DecoderHandle);
}

int  Minimp3DecoderPlugin::AudioDecoderMp3Close()
{
    return 0;
}

Status  Minimp3DecoderPlugin::AudioDecoderMp3Process(std::shared_ptr<Buffer> inBuffer, std::shared_ptr<Buffer> outBuffer)
{
    auto inData  = inBuffer->GetMemory();
    auto outData = outBuffer->GetMemory();
    Minimp3WrapperMp3decFrameInfo frameInfo;

    uint32_t probePcmLength = samplesPerFrame_ * sizeof(Mp3DecoderSample) * channels_;
    if (outData->GetCapacity() < probePcmLength) {
        return Status::ERROR_UNKNOWN;
    }
    int16_t *pcmPtr = (int16_t *)outData->GetWritableData(probePcmLength, 0);
    int sampleCount = minimp3DecoderImpl_.decoderFrame(&mp3DecoderAttr_.mp3DecoderHandle, inData->GetReadOnlyData(), inData->GetSize(), pcmPtr, &frameInfo);
    if (sampleCount > 0) {
        if (frameInfo.frame_bytes) {
            return Status::OK;
        }
    } else if (sampleCount == 0) {
        return Status::OK;
    } else {
        return Status::ERROR_UNKNOWN;
    }
}

namespace {
    std::shared_ptr<CodecPlugin> Minimp3DecoderCreator(const std::string& name)
    {
        return std::make_shared<Minimp3DecoderPlugin>(name);
    }

    Status RegisterDecoderPlugin(const std::shared_ptr<Register>& reg)
    {
        MEDIA_LOG_I("RegisterPlugins called.");
        if (!reg) {
            MEDIA_LOG_E("RegisterPlugins failed due to nullptr pointer for reg.");
            return Status::ERROR_INVALID_PARAMETER;
        }

        CodecPluginDef definition;
        definition.name      = "Minimp3DecoderPlugin";
        definition.codecType = CodecType::AUDIO_DECODER;
        definition.rank      = MAX_RANK;
        definition.creator   = Minimp3DecoderCreator;
        UpdatePluginDefinition(definition);
        if (reg->AddPlugin(definition) != Status::OK) {
            MEDIA_LOG_W("register minimp3 decoder plugin failed");
        }
        return Status::OK;
    }

    void UpdatePluginDefinition(CodecPluginDef& definition)
    {
        Capability cap("audio/unknown");
        cap.SetMime(OHOS::Media::MEDIA_MIME_AUDIO_MPEG)
                .AppendFixedKey<uint32_t>(Capability::Key::AUDIO_MPEG_VERSION, 1)
                .AppendIntervalKey<uint32_t>(Capability::Key::AUDIO_MPEG_LAYER, 1, 3);

        DiscreteCapability<uint32_t> values = {8000, 16000, 22050, 44100, 48000, 32000};
        cap.AppendDiscreteKeys(Capability::Key::AUDIO_SAMPLE_RATE, values);

        DiscreteCapability<AudioChannelLayout> channelLayoutValues = {AudioChannelLayout::MONO, AudioChannelLayout::STEREO};
        cap.AppendDiscreteKeys<AudioChannelLayout>(Capability::Key::AUDIO_CHANNEL_LAYOUT, channelLayoutValues);

        DiscreteCapability<AudioSampleFormat> sampleFmtValues = {AudioSampleFormat::S32, AudioSampleFormat::S16, AudioSampleFormat::S16P};
        cap.AppendDiscreteKeys<AudioSampleFormat>(Capability::Key::AUDIO_SAMPLE_FORMAT, sampleFmtValues);

        definition.inCaps.push_back(cap);
        definition.outCaps.emplace_back(Capability(OHOS::Media::MEDIA_MIME_AUDIO_RAW));
    }

    void UnRegisterAudioDecoderPlugin()
    {
        return;
    }
}

PLUGIN_DEFINITION(Minimp3Decoder, LicenseType::CC0, RegisterDecoderPlugin, [] {});

} // namespace Plugin
} // namespace Media
} // namespace OHOS
