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

#define HST_LOG_TAG "Ffmpeg_Au_Decoder"

#include "audio_ffmpeg_decoder_plugin.h"
#include <cstring>
#include <map>
#include <set>
#include "plugin/common/plugin_audio_tags.h"
#include "plugin/common/plugin_buffer.h"
#include "plugin/interface/codec_plugin.h"
#include "plugins/ffmpeg_adapter/utils/ffmpeg_utils.h"
#include "utils/constants.h"

namespace {
using namespace OHOS::Media::Plugin;
using namespace Ffmpeg;
void UpdatePluginDefinition(const AVCodec* codec, CodecPluginDef& definition);

std::map<std::string, std::shared_ptr<const AVCodec>> codecMap;

const size_t BUFFER_QUEUE_SIZE = 6;

const std::set<AVCodecID> g_supportedCodec = {AV_CODEC_ID_MP3, AV_CODEC_ID_FLAC, AV_CODEC_ID_AAC, AV_CODEC_ID_AAC_LATM};

Status RegisterAudioDecoderPlugins(const std::shared_ptr<Register>& reg)
{
    const AVCodec* codec = nullptr;
    void* ite = nullptr;
    MEDIA_LOG_I("registering audio decoders");
    while ((codec = av_codec_iterate(&ite))) {
        if (!av_codec_is_decoder(codec) || codec->type != AVMEDIA_TYPE_AUDIO) {
            continue;
        }
        if (g_supportedCodec.find(codec->id) == g_supportedCodec.end()) {
            MEDIA_LOG_D("codec " PUBLIC_LOG_S "(" PUBLIC_LOG_S ") is not supported right now",
                        codec->name, codec->long_name);
            continue;
        }
        CodecPluginDef definition;
        definition.name = "ffmpegAuDec_" + std::string(codec->name);
        definition.codecType = CodecType::AUDIO_DECODER;
        definition.rank = 100; // 100
        definition.creator = [] (const std::string& name) -> std::shared_ptr<CodecPlugin> {
            return std::make_shared<AudioFfmpegDecoderPlugin>(name);
        };
        UpdatePluginDefinition(codec, definition);
        // do not delete the codec in the deleter
        codecMap[definition.name] = std::shared_ptr<AVCodec>(const_cast<AVCodec*>(codec), [](void* ptr) {});
        if (reg->AddPlugin(definition) != Status::OK) {
            MEDIA_LOG_W("register plugin " PUBLIC_LOG_S " failed", definition.name.c_str());
        }
    }
    return Status::OK;
}

void UnRegisterAudioDecoderPlugin()
{
    codecMap.clear();
}

void UpdatePluginDefinition(const AVCodec* codec, CodecPluginDef& definition)
{
    Capability inputCaps("audio/unknown");
    switch (codec->id) {
        case AV_CODEC_ID_MP3:
            inputCaps.SetMime(OHOS::Media::MEDIA_MIME_AUDIO_MPEG)
                .AppendFixedKey<uint32_t>(Capability::Key::AUDIO_MPEG_VERSION, 1)
                .AppendIntervalKey<uint32_t>(Capability::Key::AUDIO_MPEG_LAYER, 1, 3); // 3
            break;
        case AV_CODEC_ID_FLAC:
            inputCaps.SetMime(OHOS::Media::MEDIA_MIME_AUDIO_FLAC);
            break;
        case AV_CODEC_ID_AAC:
            inputCaps.SetMime(OHOS::Media::MEDIA_MIME_AUDIO_AAC);
            break;
        case AV_CODEC_ID_AAC_LATM:
            inputCaps.SetMime(OHOS::Media::MEDIA_MIME_AUDIO_AAC_LATM);
            break;
        default:
            MEDIA_LOG_I("codec is not supported right now");
    }

    size_t index = 0;
    if (codec->supported_samplerates != nullptr) {
        DiscreteCapability<uint32_t> values;
        for (index = 0; codec->supported_samplerates[index] != 0; ++index) {
            values.push_back(codec->supported_samplerates[index]);
        }
        if (index) {
            inputCaps.AppendDiscreteKeys(Capability::Key::AUDIO_SAMPLE_RATE, values);
        }
    }

    if (codec->channel_layouts != nullptr) {
        DiscreteCapability<AudioChannelLayout> values;
        for (index = 0; codec->channel_layouts[index] != 0; ++index) {
            values.push_back(AudioChannelLayout(codec->channel_layouts[index]));
        }
        if (index) {
            inputCaps.AppendDiscreteKeys<AudioChannelLayout>(Capability::Key::AUDIO_CHANNEL_LAYOUT, values);
        }
    }
    definition.inCaps.push_back(inputCaps);

    Capability outputCaps(OHOS::Media::MEDIA_MIME_AUDIO_RAW);
    if (codec->sample_fmts != nullptr) {
        DiscreteCapability<AudioSampleFormat> values;
        for (index = 0; codec->sample_fmts[index] != AV_SAMPLE_FMT_NONE; ++index) {
            values.push_back(ConvFf2PSampleFmt(codec->sample_fmts[index]));
        }
        if (index) {
            outputCaps.AppendDiscreteKeys<AudioSampleFormat>(Capability::Key::AUDIO_SAMPLE_FORMAT, values);
        }
    }
    definition.outCaps.push_back(outputCaps);
}
} // namespace
PLUGIN_DEFINITION(FFmpegAudioDecoders, LicenseType::LGPL, RegisterAudioDecoderPlugins, UnRegisterAudioDecoderPlugin);

namespace OHOS {
namespace Media {
namespace Plugin {
namespace Ffmpeg {
AudioFfmpegDecoderPlugin::AudioFfmpegDecoderPlugin(std::string name) : CodecPlugin(std::move(name)) {}

AudioFfmpegDecoderPlugin::~AudioFfmpegDecoderPlugin()
{
    DeInitLocked();
}
Status AudioFfmpegDecoderPlugin::Init()
{
    auto ite = codecMap.find(pluginName_);
    if (ite == codecMap.end()) {
        MEDIA_LOG_W("cannot find codec with name " PUBLIC_LOG "s", pluginName_.c_str());
        return Status::ERROR_UNSUPPORTED_FORMAT;
    }
    {
        OSAL::ScopedLock lock(avMutex_);
        avCodec_ = ite->second;
        cachedFrame_ = std::shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame *fp) { av_frame_free(&fp); });
    }
    OSAL::ScopedLock lock1(parameterMutex_);
    {
        audioParameter_[Tag::REQUIRED_OUT_BUFFER_CNT] = (uint32_t) BUFFER_QUEUE_SIZE;
    }
    return Status::OK;
}

Status AudioFfmpegDecoderPlugin::Deinit()
{
    OSAL::ScopedLock lock(avMutex_);
    OSAL::ScopedLock lock1(parameterMutex_);
    return DeInitLocked();
}

Status AudioFfmpegDecoderPlugin::DeInitLocked()
{
    ResetLocked();
    avCodec_.reset();
    cachedFrame_.reset();
    return Status::OK;
}

Status AudioFfmpegDecoderPlugin::SetParameter(Tag tag, const ValueType& value)
{
    OSAL::ScopedLock lock(parameterMutex_);
    audioParameter_.insert(std::make_pair(tag, value));
    return Status::OK;
}

Status AudioFfmpegDecoderPlugin::GetParameter(Tag tag, ValueType& value)
{
    OSAL::ScopedLock lock(parameterMutex_);
    auto res = audioParameter_.find(tag);
    if (res != audioParameter_.end()) {
        value = res->second;
        return Status::OK;
    }
    return Status::ERROR_INVALID_PARAMETER;
}

template <typename T>
Status AudioFfmpegDecoderPlugin::FindInParameterMapThenAssignLocked(Tag tag, T& assign)
{
    if (audioParameter_.count(tag) == 0) {
        MEDIA_LOG_I("tag " PUBLIC_LOG_D32 "is not set", static_cast<int32_t>(tag));
        return Status::OK;
    }
    const auto& item = audioParameter_.at(tag);
    if (item.SameTypeWith(typeid(T))) {
        assign = Plugin::AnyCast<T>(item);
        return Status::OK;
    }
    MEDIA_LOG_E("type of tag " PUBLIC_LOG_D32 "not matched", static_cast<int32_t>(tag));
    return Status::ERROR_MISMATCHED_TYPE;
}

Status AudioFfmpegDecoderPlugin::Prepare()
{
#define FAIL_RET_WHEN_ASSIGN_LOCKED(tag, type, assign) \
do { \
    type tmpVal; \
    auto ret = FindInParameterMapThenAssignLocked(tag, tmpVal); \
    if (ret != Status::OK) { \
        return ret; \
    } \
    (assign) = tmpVal; \
} while (0)
    AVCodecContext* context = nullptr;
    {
        OSAL::ScopedLock lock(avMutex_);
        if (avCodec_ == nullptr) {
            return Status::ERROR_WRONG_STATE;
        }
        context = avcodec_alloc_context3(avCodec_.get());
    }
    FALSE_RET_V_MSG_E(context != nullptr, Status::ERROR_NO_MEMORY, "can't allocate codec context");
    auto tmpCtx = std::shared_ptr<AVCodecContext>(context, [](AVCodecContext* ptr) {
        avcodec_free_context(&ptr);
    });
    {
        OSAL::ScopedLock lock1(parameterMutex_);
        FAIL_RET_WHEN_ASSIGN_LOCKED(Tag::AUDIO_CHANNELS, uint32_t, tmpCtx->channels);
        FAIL_RET_WHEN_ASSIGN_LOCKED(Tag::AUDIO_SAMPLE_RATE, uint32_t, tmpCtx->sample_rate);
        FAIL_RET_WHEN_ASSIGN_LOCKED(Tag::MEDIA_BITRATE, int64_t, tmpCtx->bit_rate);
        AudioSampleFormat audioSampleFormat = AudioSampleFormat::NONE;
        auto ret = FindInParameterMapThenAssignLocked(Tag::AUDIO_SAMPLE_FORMAT, audioSampleFormat);
        if (ret != Status::OK) {
            return ret;
        }
        auto tmpFmt = ConvP2FfSampleFmt(Plugin::AnyCast<AudioSampleFormat>(audioSampleFormat));
        if (tmpFmt == AV_SAMPLE_FMT_NONE) {
            return Status::ERROR_INVALID_PARAMETER;
        }
        tmpCtx->sample_fmt = tmpFmt;
        tmpCtx->request_sample_fmt = tmpCtx->sample_fmt;
        ret = AssignExtraDataIfExistsLocked(tmpCtx);
        if (ret != Status::OK) {
            return ret;
        }
    }
    tmpCtx->workaround_bugs = static_cast<uint32_t>(tmpCtx->workaround_bugs) | static_cast<uint32_t>(FF_BUG_AUTODETECT);
    tmpCtx->err_recognition = 1;
    {
        OSAL::ScopedLock lock(avMutex_);
        avCodecContext_ = tmpCtx;
    }
    return Status::OK;
#undef FAIL_RET_WHEN_ASSIGN_LOCKED
}

Status AudioFfmpegDecoderPlugin::AssignExtraDataIfExistsLocked(const std::shared_ptr<AVCodecContext>& ctx)
{
    if (!ctx) {
        return Status::ERROR_INVALID_PARAMETER;
    }
    if (audioParameter_.count(Tag::MEDIA_CODEC_CONFIG) == 0) {
        return Status::OK;
    }
    const auto& item = audioParameter_.at(Tag::MEDIA_CODEC_CONFIG);
    if (!item.SameTypeWith(typeid(CodecConfig))) {
        return Status::ERROR_MISMATCHED_TYPE;
    }
    const auto* codecConfig = Plugin::AnyCast<CodecConfig>(&item);
    if (!codecConfig->empty()) {
        auto configSize = codecConfig->size();
        auto allocSize = AlignUp(configSize + AV_INPUT_BUFFER_PADDING_SIZE, 16); // 16
        ctx->extradata = static_cast<uint8_t*>(av_mallocz(allocSize));
        if (!ctx->extradata) {
            return Status::ERROR_NO_MEMORY;
        }
        if (memcpy_s(ctx->extradata, allocSize, codecConfig->data(), configSize) != EOK) {
            MEDIA_LOG_E("init codec context extra data error");
            av_free(ctx->extradata);
            ctx->extradata = nullptr;
            return Status::ERROR_UNKNOWN;
        }
        ctx->extradata_size = configSize;
    }
    return Status::OK;
}

Status AudioFfmpegDecoderPlugin::ResetLocked()
{
    audioParameter_.clear();
    StopLocked();
    avCodecContext_.reset();
    return Status::OK;
}

Status AudioFfmpegDecoderPlugin::Reset()
{
    OSAL::ScopedLock lock(avMutex_);
    OSAL::ScopedLock lock1(parameterMutex_);
    return ResetLocked();
}

Status AudioFfmpegDecoderPlugin::OpenCtxLocked()
{
    if (avCodecContext_ == nullptr) {
        return Status::ERROR_WRONG_STATE;
    }
    auto res = avcodec_open2(avCodecContext_.get(), avCodec_.get(), nullptr);
    if (res != 0) {
        MEDIA_LOG_E("avcodec open error " PUBLIC_LOG_S, AVStrError(res).c_str());
        return Status::ERROR_UNKNOWN;
    }
    return Status::OK;
}

Status AudioFfmpegDecoderPlugin::Start()
{
    OSAL::ScopedLock lock(avMutex_);
    return OpenCtxLocked();
}

Status AudioFfmpegDecoderPlugin::CloseCtxLocked()
{
    if (avCodecContext_ != nullptr) {
        auto res = avcodec_close(avCodecContext_.get());
        if (res != 0) {
            MEDIA_LOG_E("avcodec close error " PUBLIC_LOG_S, AVStrError(res).c_str());
            return Status::ERROR_UNKNOWN;
        }
    }
    return Status::OK;
}

Status AudioFfmpegDecoderPlugin::StopLocked()
{
    auto ret = CloseCtxLocked();
    avCodecContext_.reset();
    if (outBuffer_) {
        outBuffer_.reset();
    }
    return ret;
}

Status AudioFfmpegDecoderPlugin::Stop()
{
    OSAL::ScopedLock lock(avMutex_);
    return StopLocked();
}

Status AudioFfmpegDecoderPlugin::Flush()
{
    MEDIA_LOG_I("Flush entered.");
    OSAL::ScopedLock lock(avMutex_);
    if (avCodecContext_ != nullptr) {
        avcodec_flush_buffers(avCodecContext_.get());
    }
    MEDIA_LOG_I("Flush exit.");
    return Status::OK;
}

Status AudioFfmpegDecoderPlugin::QueueInputBuffer(const std::shared_ptr<Buffer>& inputBuffer, int32_t timeoutMs)
{
    MEDIA_LOG_D("queue input buffer");
    (void)timeoutMs;
    if (inputBuffer->IsEmpty() && !(inputBuffer->flag & BUFFER_FLAG_EOS)) {
        MEDIA_LOG_E("decoder does not support fd buffer");
        return Status::ERROR_INVALID_DATA;
    }
    Status ret = Status::OK;
    {
        OSAL::ScopedLock lock(avMutex_);
        if (avCodecContext_ == nullptr) {
            return Status::ERROR_WRONG_STATE;
        }
        ret = SendBufferLocked(inputBuffer);
    }
    return ret;
}

Status AudioFfmpegDecoderPlugin::QueueOutputBuffer(const std::shared_ptr<Buffer>& outputBuffer, int32_t timeoutMs)
{
    MEDIA_LOG_D("queue output buffer");
    (void)timeoutMs;
    if (!outputBuffer) {
        return Status::ERROR_INVALID_PARAMETER;
    }
    outBuffer_ = outputBuffer;
    return SendOutputBuffer();
}

Status AudioFfmpegDecoderPlugin::SendOutputBuffer()
{
    MEDIA_LOG_D("send output buffer");
    Status status = ReceiveBuffer();
    if (status == Status::OK || status == Status::END_OF_STREAM) {
        dataCallback_->OnOutputBufferDone(outBuffer_);
    }
    outBuffer_.reset();
    return status;
}

Status AudioFfmpegDecoderPlugin::SendBufferLocked(const std::shared_ptr<Buffer>& inputBuffer)
{
    AVPacket packet;
    size_t bufferLength = 0;
    bool eos = false;
    if (inputBuffer == nullptr || (inputBuffer->flag & BUFFER_FLAG_EOS) != 0) {
        // eos buffer
        eos = true;
    } else {
        auto inputMemory = inputBuffer->GetMemory();
        const uint8_t* ptr = inputMemory->GetReadOnlyData();
        bufferLength = inputMemory->GetSize();
        // pad to data if needed
        if (bufferLength % AV_INPUT_BUFFER_PADDING_SIZE != 0) {
            if (paddedBufferSize_ < bufferLength + AV_INPUT_BUFFER_PADDING_SIZE) {
                paddedBufferSize_ = bufferLength + AV_INPUT_BUFFER_PADDING_SIZE;
                paddedBuffer_.reserve(paddedBufferSize_);
                MEDIA_LOG_I("increase padded buffer size to " PUBLIC_LOG "zu", paddedBufferSize_);
            }
            paddedBuffer_.assign(ptr, ptr + bufferLength);
            paddedBuffer_.insert(paddedBuffer_.end(), AV_INPUT_BUFFER_PADDING_SIZE, 0);
            ptr = paddedBuffer_.data();
        }
        av_init_packet(&packet);
        packet.data = const_cast<uint8_t*>(ptr);
        packet.size = bufferLength;
        packet.pts = inputBuffer->pts;
    }
    AVPacket* packetPtr = nullptr;
    if (!eos) {
        packetPtr = &packet;
    }
    auto ret = avcodec_send_packet(avCodecContext_.get(), packetPtr);
    if (ret == 0) {
        return Status::OK;
    } else if (ret == AVERROR(EAGAIN)) {
        return Status::ERROR_AGAIN;
    } else {
        MEDIA_LOG_E("send buffer error " PUBLIC_LOG "s", AVStrError(ret).c_str());
        return Status::ERROR_UNKNOWN;
    }
}

Status AudioFfmpegDecoderPlugin::ReceiveFrameSucc(const std::shared_ptr<Buffer>& ioInfo)
{
    int32_t channels = cachedFrame_->channels;
    int32_t samples = cachedFrame_->nb_samples;
    auto sampleFormat = static_cast<AVSampleFormat>(cachedFrame_->format);
    int32_t bytePerSample = av_get_bytes_per_sample(sampleFormat);
    int32_t outputSize = samples * bytePerSample * channels;
    auto ioInfoMem = ioInfo->GetMemory();
    if (ioInfoMem->GetCapacity() < outputSize) {
        MEDIA_LOG_W("output buffer size is not enough");
        return Status::ERROR_NO_MEMORY;
    }
    if (av_sample_fmt_is_planar(avCodecContext_->sample_fmt)) {
        int32_t planarSize = outputSize / channels;
        for (size_t idx = 0; idx < channels; idx++) {
            ioInfoMem->Write(cachedFrame_->extended_data[idx], planarSize);
        }
    } else {
        ioInfoMem->Write(cachedFrame_->data[0], outputSize);
    }
    ioInfo->pts = static_cast<uint64_t>(cachedFrame_->pts);
    return Status::OK;
}

Status AudioFfmpegDecoderPlugin::ReceiveBufferLocked(const std::shared_ptr<Buffer>& ioInfo)
{
    Status status;
    auto ret = avcodec_receive_frame(avCodecContext_.get(), cachedFrame_.get());
    if (ret >= 0) {
        MEDIA_LOG_D("receive one frame");
        status = ReceiveFrameSucc(ioInfo);
    } else if (ret == AVERROR_EOF) {
        MEDIA_LOG_I("eos received");
        ioInfo->GetMemory()->Reset();
        ioInfo->flag = BUFFER_FLAG_EOS;
        status = Status::END_OF_STREAM;
    } else if (ret == AVERROR(EAGAIN)) {
        status = Status::ERROR_NOT_ENOUGH_DATA;
    } else {
        MEDIA_LOG_E("audio decoder receive error: " PUBLIC_LOG "s", AVStrError(ret).c_str());
        status = Status::ERROR_UNKNOWN;
    }
    av_frame_unref(cachedFrame_.get());
    return status;
}

Status AudioFfmpegDecoderPlugin::ReceiveBuffer()
{
    std::shared_ptr<Buffer> ioInfo = outBuffer_;
    if ((ioInfo == nullptr) || ioInfo->IsEmpty() ||
        (ioInfo->GetBufferMeta()->GetType() != BufferMetaType::AUDIO)) {
        MEDIA_LOG_W("cannot fetch valid buffer to output");
        return Status::ERROR_NO_MEMORY;
    }
    Status status;
    {
        OSAL::ScopedLock l(avMutex_);
        if (avCodecContext_ == nullptr) {
            return Status::ERROR_WRONG_STATE;
        }
        status = ReceiveBufferLocked(ioInfo);
    }
    return status;
}

std::shared_ptr<Allocator> AudioFfmpegDecoderPlugin::GetAllocator()
{
    return nullptr;
}
} // Ffmpeg
} // namespace Plugin
} // namespace Media
} // namespace OHOS
