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

#ifdef RECORDER_SUPPORT

#define HST_LOG_TAG "AudioCapturePlugin"

#include "audio_capture_plugin.h"
#include <cmath>
#include "foundation/log.h"
#include "utils/utils.h"
#include "utils/constants.h"

namespace {
// register plugins
using namespace OHOS::Media::Plugin;

#define MAX_CAPTURE_BUFFER_SIZE 100000
#define TIME_SEC_TO_NS          1000000000

std::map<AudioSampleFormat, OHOS::AudioStandard::AudioSampleFormat> g_formatMap = {
    {AudioSampleFormat::S8, OHOS::AudioStandard::AudioSampleFormat::INVALID_WIDTH},
    {AudioSampleFormat::U8, OHOS::AudioStandard::AudioSampleFormat::SAMPLE_U8},
    {AudioSampleFormat::S8P, OHOS::AudioStandard::AudioSampleFormat::INVALID_WIDTH},
    {AudioSampleFormat::U8P, OHOS::AudioStandard::AudioSampleFormat::INVALID_WIDTH},
    {AudioSampleFormat::S16, OHOS::AudioStandard::AudioSampleFormat::SAMPLE_S16LE},
    {AudioSampleFormat::U16, OHOS::AudioStandard::AudioSampleFormat::INVALID_WIDTH},
    {AudioSampleFormat::S16P, OHOS::AudioStandard::AudioSampleFormat::INVALID_WIDTH},
    {AudioSampleFormat::U16P, OHOS::AudioStandard::AudioSampleFormat::INVALID_WIDTH},
    {AudioSampleFormat::S24, OHOS::AudioStandard::AudioSampleFormat::SAMPLE_S24LE},
    {AudioSampleFormat::U24, OHOS::AudioStandard::AudioSampleFormat::INVALID_WIDTH},
    {AudioSampleFormat::S24P, OHOS::AudioStandard::AudioSampleFormat::INVALID_WIDTH},
    {AudioSampleFormat::U24P, OHOS::AudioStandard::AudioSampleFormat::INVALID_WIDTH},
    {AudioSampleFormat::S32, OHOS::AudioStandard::AudioSampleFormat::SAMPLE_S32LE},
    {AudioSampleFormat::U32P, OHOS::AudioStandard::AudioSampleFormat::INVALID_WIDTH},
    {AudioSampleFormat::S32P, OHOS::AudioStandard::AudioSampleFormat::INVALID_WIDTH},
    {AudioSampleFormat::U32P, OHOS::AudioStandard::AudioSampleFormat::INVALID_WIDTH},
    {AudioSampleFormat::S64, OHOS::AudioStandard::AudioSampleFormat::INVALID_WIDTH},
    {AudioSampleFormat::U64P, OHOS::AudioStandard::AudioSampleFormat::INVALID_WIDTH},
    {AudioSampleFormat::S64P, OHOS::AudioStandard::AudioSampleFormat::INVALID_WIDTH},
    {AudioSampleFormat::U64P, OHOS::AudioStandard::AudioSampleFormat::INVALID_WIDTH},
    {AudioSampleFormat::F32, OHOS::AudioStandard::AudioSampleFormat::INVALID_WIDTH},
    {AudioSampleFormat::F32P, OHOS::AudioStandard::AudioSampleFormat::INVALID_WIDTH},
    {AudioSampleFormat::F64, OHOS::AudioStandard::AudioSampleFormat::INVALID_WIDTH},
    {AudioSampleFormat::F64P, OHOS::AudioStandard::AudioSampleFormat::INVALID_WIDTH},
};

std::shared_ptr<SourcePlugin> AudioCapturePluginCreater(const std::string &name)
{
    return std::make_shared<AudioCapturePlugin>(name);
}

void UpdateSupportedSampleRate(Capability &outCaps)
{
    auto supportedSampleRateList = OHOS::AudioStandard::AudioCapturer::GetSupportedSamplingRates();
    if (!supportedSampleRateList.empty()) {
        DiscreteCapability<uint32_t> values;
        for (auto iter = supportedSampleRateList.begin(); iter != supportedSampleRateList.end(); ++iter) {
            if (static_cast<int32_t>(*iter) <= 0) {
                continue;
            }
            values.push_back(static_cast<uint32_t>(*iter));
        }
        if (!values.empty()) {
            outCaps.AppendDiscreteKeys<uint32_t>(Capability::Key::AUDIO_SAMPLE_RATE, values);
        }
    }
}

void UpdateSupportedChannels(Capability &outCaps)
{
    auto supportedChannelsList = OHOS::AudioStandard::AudioCapturer::GetSupportedChannels();
    if (!supportedChannelsList.empty()) {
        DiscreteCapability<uint32_t> values;
        for (auto iter = supportedChannelsList.begin(); iter != supportedChannelsList.end(); ++iter) {
            if (static_cast<int32_t>(*iter) <= 0) {
                continue;
            }
            values.push_back(static_cast<uint32_t>(*iter));
        }
        if (!values.empty()) {
            outCaps.AppendDiscreteKeys<uint32_t>(Capability::Key::AUDIO_CHANNELS, values);
        }
    }
}

void UpdateSupportedSampleFormat(Capability &outCaps)
{
    auto supportedFormatsList = OHOS::AudioStandard::AudioCapturer::GetSupportedFormats();
    if (!supportedFormatsList.empty()) {
        DiscreteCapability<AudioSampleFormat> values;
        for (auto iter = supportedFormatsList.begin(); iter != supportedFormatsList.end(); ++iter) {
            auto sampleFormat = static_cast<OHOS::AudioStandard::AudioSampleFormat>(*iter);
            switch (sampleFormat) {
                case OHOS::AudioStandard::AudioSampleFormat::SAMPLE_U8:
                    values.push_back(AudioSampleFormat::U8);
                    break;
                case OHOS::AudioStandard::AudioSampleFormat::SAMPLE_S16LE:
                    values.push_back(AudioSampleFormat::S16);
                    break;
                case OHOS::AudioStandard::AudioSampleFormat::SAMPLE_S24LE:
                    values.push_back(AudioSampleFormat::S24);
                    break;
                case OHOS::AudioStandard::AudioSampleFormat::SAMPLE_S32LE:
                    values.push_back(AudioSampleFormat::S32);
                    break;
                default:
                    break;
            }
        }
        if (!values.empty()) {
            outCaps.AppendDiscreteKeys<AudioSampleFormat>(Capability::Key::AUDIO_SAMPLE_FORMAT, values);
        }
    }
}

const Status AudioCaptureRegister(const std::shared_ptr<Register> &reg)
{
    SourcePluginDef definition;
    definition.name = "AudioCapture";
    definition.description = "Audio capture form audio service";
    definition.rank = 100; // 100: max rank
    definition.inputType = "mic";
    definition.creator = AudioCapturePluginCreater;
    Capability outCaps(OHOS::Media::MEDIA_MIME_AUDIO_RAW);
    UpdateSupportedSampleRate(outCaps);
    UpdateSupportedChannels(outCaps);
    UpdateSupportedSampleFormat(outCaps);
    definition.outCaps.push_back(outCaps);
    // add es outCaps later
    return reg->AddPlugin(definition);
}

PLUGIN_DEFINITION(AudioCapture, LicenseType::APACHE_V2, AudioCaptureRegister, [] {});
}

namespace OHOS {
namespace Media {
namespace Plugin {
void* AudioCaptureAllocator::Alloc(size_t size)
{
    if (size == 0) {
        return nullptr;
    }
    return reinterpret_cast<void*>(new (std::nothrow) uint8_t[size]); // NOLINT: cast
}

void AudioCaptureAllocator::Free(void* ptr) // NOLINT: void*
{
    if (ptr != nullptr) {
        delete[](uint8_t*) ptr;
    }
}

AudioCapturePlugin::AudioCapturePlugin(std::string name)
    : SourcePlugin(std::move(name)),
      bufferSize_(0),
      curTimestamp_(0),
      stopTimestamp_(0),
      totalPauseTime_(0),
      bitRate_(0),
      isStop_(false)
{
    MEDIA_LOG_D("IN");
}

AudioCapturePlugin::~AudioCapturePlugin()
{
    MEDIA_LOG_D("IN");
}

Status AudioCapturePlugin::Init()
{
    MEDIA_LOG_D("IN");
    mAllocator_ = std::make_shared<AudioCaptureAllocator>();
    if (audioCapturer_ == nullptr) {
        audioCapturer_ = AudioStandard::AudioCapturer::Create(AudioStandard::AudioStreamType::STREAM_MUSIC);
        if (audioCapturer_ == nullptr) {
            MEDIA_LOG_E("Create audioCapturer fail");
            return Status::ERROR_UNKNOWN;
        }
    }
    return Status::OK;
}

Status AudioCapturePlugin::Deinit()
{
    MEDIA_LOG_D("IN");
    if (audioCapturer_) {
        if (audioCapturer_->GetStatus() == AudioStandard::CapturerState::CAPTURER_RUNNING) {
            if (!audioCapturer_->Stop()) {
                MEDIA_LOG_E("Stop audioCapturer fail");
            }
        }
        if (!audioCapturer_->Release()) {
            MEDIA_LOG_E("Release audioCapturer fail");
        }
        audioCapturer_ = nullptr;
    }
    return Status::OK;
}

Status AudioCapturePlugin::Prepare()
{
    MEDIA_LOG_D("IN");
    AudioStandard::AudioCapturerParams params;
    params.audioEncoding = AudioStandard::ENCODING_INVALID;
    auto supportedEncodingTypes = OHOS::AudioStandard::AudioCapturer::GetSupportedEncodingTypes();
    for (auto iter = supportedEncodingTypes.begin(); iter != supportedEncodingTypes.end(); ++iter) {
        if (static_cast<OHOS::AudioStandard::AudioEncodingType>(*iter) == AudioStandard::ENCODING_PCM) {
            params.audioEncoding = AudioStandard::ENCODING_PCM;
            break;
        }
    }
    if (params.audioEncoding != AudioStandard::ENCODING_PCM) {
        MEDIA_LOG_E("audioCapturer do not support pcm encoding");
        return Status::ERROR_UNKNOWN;
    }
    if (audioCapturer_->SetParams(params)) {
        MEDIA_LOG_E("audioCapturer SetParams() fail");
        return Status::ERROR_UNKNOWN;
    }
    size_t size;
    if (audioCapturer_->GetBufferSize(size)) {
        MEDIA_LOG_E("audioCapturer GetBufferSize() fail");
        return Status::ERROR_UNKNOWN;
    }
    if (size >= MAX_CAPTURE_BUFFER_SIZE) {
        MEDIA_LOG_E("bufferSize is too big: %zu", size);
        return Status::ERROR_INVALID_PARAMETER;
    }
    bufferSize_ = size;
    MEDIA_LOG_D("bufferSize is: %zu", bufferSize_);
    return Status::OK;
}

Status AudioCapturePlugin::Reset()
{
    MEDIA_LOG_D("IN");
    if (audioCapturer_->GetStatus() == AudioStandard::CapturerState::CAPTURER_RUNNING) {
        if (!audioCapturer_->Stop()) {
            MEDIA_LOG_E("Stop audioCapturer fail");
            return Status::ERROR_UNKNOWN;
        }
    }
    bufferSize_ = 0;
    curTimestamp_ = 0;
    stopTimestamp_ = 0;
    totalPauseTime_ = 0;
    bitRate_ = 0;
    isStop_ = false;
    return Status::OK;
}

Status AudioCapturePlugin::Start()
{
    MEDIA_LOG_D("IN");
    if (audioCapturer_->GetStatus() != AudioStandard::CapturerState::CAPTURER_PREPARED) {
        MEDIA_LOG_E("audioCapturer need to prepare first");
        return Status::ERROR_WRONG_STATE;
    }
    if (!audioCapturer_->Start()) {
        MEDIA_LOG_E("Start audioCapturer fail");
        return Status::ERROR_UNKNOWN;
    }
    if (isStop_) {
        if (GetAudioTime(curTimestamp_) != Status::OK) {
            MEDIA_LOG_E("Get auido time fail");
        }
        if (curTimestamp_ < stopTimestamp_) {
            MEDIA_LOG_E("Get wrong audio time");
        }
        totalPauseTime_ += std::fabs(curTimestamp_ - stopTimestamp_);
        isStop_ = false;
    }
    return Status::OK;
}

Status AudioCapturePlugin::Stop()
{
    MEDIA_LOG_D("IN");
    enum AudioStandard::CapturerState state = audioCapturer_->GetStatus();
    if (state != AudioStandard::CapturerState::CAPTURER_RUNNING) {
        MEDIA_LOG_E("audioCapturer need to prepare first");
        return Status::ERROR_WRONG_STATE;
    }
    if (!audioCapturer_->Stop()) {
        MEDIA_LOG_E("Stop audioCapturer fail");
        return Status::ERROR_UNKNOWN;
    }
    if (!isStop_) {
        stopTimestamp_ = curTimestamp_;
        isStop_ = true;
    }
    return Status::OK;
}

bool AudioCapturePlugin::IsParameterSupported(Tag tag)
{
    MEDIA_LOG_D("IN");
    return false;
}

Status AudioCapturePlugin::GetParameter(Tag tag, ValueType& value)
{
    MEDIA_LOG_D("IN");
    AudioStandard::AudioCapturerParams params;
    if (!audioCapturer_ || !audioCapturer_->GetParams(params)) {
        MEDIA_LOG_E("audioCapturer GetParams() fail");
        return Status::ERROR_UNKNOWN;
    }
    switch (tag) {
        case Tag::AUDIO_SAMPLE_RATE: {
            if (params.samplingRate != capturerParams_.samplingRate) {
                MEDIA_LOG_W("samplingRate has changed from %u to %u",
                            capturerParams_.samplingRate, params.samplingRate);
            }
            value = params.samplingRate;
            break;
        }
        case Tag::AUDIO_CHANNELS: {
            if (params.audioChannel != capturerParams_.audioChannel) {
                MEDIA_LOG_W("audioChannel has changed from %u to %u",
                            capturerParams_.audioChannel, params.audioChannel);
            }
            value = params.audioChannel;
            break;
        }
        case Tag::MEDIA_BITRATE: {
            value = bitRate_;
            break;
        }
        case Tag::AUDIO_SAMPLE_FORMAT: {
            if (params.audioSampleFormat != capturerParams_.audioSampleFormat) {
                MEDIA_LOG_W("audioSampleFormat has changed from %u to %u",
                            capturerParams_.audioSampleFormat, params.audioSampleFormat);
            }
            value = params.audioSampleFormat;
            break;
        }
        default:
            MEDIA_LOG_I("Unknown key");
            break;
    }
    return Status::OK;
}

bool AudioCapturePlugin::IsSampleRateSupported(const uint32_t sampleRate)
{
    auto supportedSampleRateList = OHOS::AudioStandard::AudioCapturer::GetSupportedSamplingRates();
    if (supportedSampleRateList.empty()) {
        MEDIA_LOG_E("GetSupportedSamplingRates() fail");
        return false;
    }
    for (auto iter = supportedSampleRateList.begin(); iter != supportedSampleRateList.end(); ++iter) {
        if (static_cast<int32_t>(*iter) < 0) {
            continue;
        }
        auto supportedSampleRate = static_cast<uint32_t>(*iter);
        if (sampleRate == supportedSampleRate) {
            capturerParams_.samplingRate = static_cast<OHOS::AudioStandard::AudioSamplingRate>(sampleRate);
            MEDIA_LOG_D("samplingRate: %u", capturerParams_.samplingRate);
            return true;
        }
    }
    return false;
}

bool AudioCapturePlugin::IsChannelNumSupported(const uint32_t channelNum)
{
    if (channelNum > 2) { // 2
        MEDIA_LOG_E("Unsupported channelNum: %d");
    }
    auto supportedChannelsList = OHOS::AudioStandard::AudioCapturer::GetSupportedChannels();
    if (supportedChannelsList.empty()) {
        MEDIA_LOG_E("GetSupportedChannels() fail");
        return false;
    }
    for (auto iter = supportedChannelsList.begin(); iter != supportedChannelsList.end(); ++iter) {
        if (static_cast<int32_t>(*iter) < 0) {
            continue;
        }
        auto supportedChannels = static_cast<uint32_t>(*iter);
        if (channelNum == supportedChannels) {
            capturerParams_.audioChannel = static_cast<OHOS::AudioStandard::AudioChannel>(channelNum);
            MEDIA_LOG_D("audioChannel: %u", capturerParams_.audioChannel);
            return true;
        }
    }
    return false;
}

bool AudioCapturePlugin::IsSampleFormatSupported(const OHOS::AudioStandard::AudioSampleFormat sampleFormat)
{
    auto supportedFormatsList = OHOS::AudioStandard::AudioCapturer::GetSupportedFormats();
    if (supportedFormatsList.empty()) {
        MEDIA_LOG_E("GetSupportedFormats() fail");
        return false;
    }
    for (auto iter = supportedFormatsList.begin(); iter != supportedFormatsList.end(); ++iter) {
        if (static_cast<int32_t>(*iter) < 0) {
            continue;
        }
        auto supportedChannels = static_cast<OHOS::AudioStandard::AudioSampleFormat>(*iter);
        if (sampleFormat == supportedChannels) {
            capturerParams_.audioSampleFormat = supportedChannels;
            MEDIA_LOG_D("audioSampleFormat: %u", capturerParams_.audioSampleFormat);
            return true;
        }
    }
    return false;
}

Status AudioCapturePlugin::SetParameter(Tag tag, const ValueType& value)
{
    switch (tag) {
        case Tag::AUDIO_SAMPLE_RATE: {
            if (value.Type() == typeid(uint32_t)) {
                auto sampleRate = Plugin::AnyCast<uint32_t>(value);
                if (!IsSampleRateSupported(sampleRate)) {
                    MEDIA_LOG_E("sampleRate is not supported by audiocapturer");
                    return Status::ERROR_INVALID_PARAMETER;
                }
            }
            break;
        }
        case Tag::AUDIO_CHANNELS: {
            if (value.Type() == typeid(uint32_t)) {
                auto channelNum = Plugin::AnyCast<uint32_t>(value);
                if (!IsChannelNumSupported(channelNum)) {
                    MEDIA_LOG_E("channelNum is not supported by audiocapturer");
                    return Status::ERROR_INVALID_PARAMETER;
                }
            }
            break;
        }
        case Tag::MEDIA_BITRATE: {
            if (value.Type() == typeid(int64_t)) {
                bitRate_ = Plugin::AnyCast<int64_t>(value);
                MEDIA_LOG_D("bitRate_: %u", bitRate_);
            }
            break;
        }
        case Tag::AUDIO_SAMPLE_FORMAT: {
            if (value.Type() == typeid(AudioSampleFormat)) {
                OHOS::AudioStandard::AudioSampleFormat sampleFormat =
                        g_formatMap[Plugin::AnyCast<AudioSampleFormat>(value)];
                if (sampleFormat == OHOS::AudioStandard::AudioSampleFormat::INVALID_WIDTH ||
                    !IsSampleFormatSupported(sampleFormat)) {
                    MEDIA_LOG_E("sampleFormat is not supported by audiocapturer");
                    return Status::ERROR_INVALID_PARAMETER;
                }
            }
            break;
        }
        default:
            MEDIA_LOG_I("Unknown key");
            break;
    }
    return Status::OK;
}

std::shared_ptr<Allocator> AudioCapturePlugin::GetAllocator()
{
    MEDIA_LOG_D("IN");
    return mAllocator_;
}

Status AudioCapturePlugin::SetCallback(Callback* cb)
{
    MEDIA_LOG_D("IN");
    UNUSED_VARIABLE(cb);
    return Status::ERROR_UNIMPLEMENTED;
}

Status AudioCapturePlugin::SetSource(std::string& uri, std::shared_ptr<std::map<std::string, ValueType>> params)
{
    UNUSED_VARIABLE(uri);
    UNUSED_VARIABLE(params);
    return Status::ERROR_UNIMPLEMENTED;
}

Status AudioCapturePlugin::GetAudioTime(uint64_t &audioTime)
{
    if (!audioCapturer_) {
        return Status::ERROR_WRONG_STATE;
    }
    OHOS::AudioStandard::Timestamp timeStamp;
    auto timeBase = OHOS::AudioStandard::Timestamp::Timestampbase::MONOTONIC;
    if (!audioCapturer_->GetAudioTime(timeStamp, timeBase)) {
        MEDIA_LOG_E("audioCapturer GetAudioTime() fail");
        return Status::ERROR_UNKNOWN;
    }
    if (timeStamp.time.tv_sec < 0 || timeStamp.time.tv_nsec < 0) {
        return Status::ERROR_INVALID_PARAMETER;
    }
    if ((UINT64_MAX - timeStamp.time.tv_nsec) / TIME_SEC_TO_NS > timeStamp.time.tv_sec) {
        return Status::ERROR_INVALID_PARAMETER;
    }
    audioTime = timeStamp.time.tv_sec * TIME_SEC_TO_NS + timeStamp.time.tv_nsec;
    return Status::OK;
}

Status AudioCapturePlugin::Read(std::shared_ptr<Buffer>& buffer, size_t expectedLen)
{
    auto bufferMeta = buffer->GetBufferMeta();
    if (!bufferMeta || bufferMeta->GetType() != BufferMetaType::AUDIO) {
        return Status::ERROR_INVALID_PARAMETER;
    }
    std::shared_ptr<Memory> bufData;
    if (buffer->IsEmpty()) {
        bufData = buffer->AllocMemory(GetAllocator(), expectedLen);
    } else {
        bufData = buffer->GetMemory();
    }
    if (bufData->GetSize() <= 0) {
        return Status::ERROR_NO_MEMORY;
    }
    bool isBlocking = true;
    auto size = audioCapturer_->Read(bufData->GetWritableData(expectedLen), expectedLen, isBlocking);
    if (size <= 0) {
        MEDIA_LOG_D("audioCapturer Read() fail");
        return Status::ERROR_NOT_ENOUGH_DATA;
    }
    auto ret = GetAudioTime(curTimestamp_);
    if (ret != Status::OK) {
        MEDIA_LOG_E("Get audio timestamp fail");
        return ret;
    }
    buffer->pts = curTimestamp_ + totalPauseTime_;
    bufferSize_ = size;
    return ret;
}

Status AudioCapturePlugin::GetSize(size_t& size)
{
    if (bufferSize_ == 0) {
        return Status::ERROR_INVALID_PARAMETER;
    }
    size = bufferSize_;
    MEDIA_LOG_D("bufferSize_: %zu", size);
    return Status::OK;
}

bool AudioCapturePlugin::IsSeekable()
{
    return false;
}

Status AudioCapturePlugin::SeekTo(uint64_t offset)
{
    UNUSED_VARIABLE(offset);
    return Status::ERROR_UNIMPLEMENTED;
}
} // namespace Plugin
} // namespace Media
} // namespace OHOS
#endif
