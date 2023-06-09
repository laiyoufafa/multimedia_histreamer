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

#ifndef HISTREAMER_HIPLAYER_IMPL_H
#define HISTREAMER_HIPLAYER_IMPL_H

#include <memory>
#include <unordered_map>

#include "common/any.h"
#ifdef VIDEO_SUPPORT
#include "filters/codec/video_decoder/video_decoder_filter.h"
#include "filters/sink/video_sink/video_sink_filter.h"
#endif
#include "filters/demux/demuxer_filter.h"
#include "filters/source/media_source/media_source_filter.h"
#include "internal/state_machine.h"
#include "osal/thread/condition_variable.h"
#include "osal/thread/mutex.h"
#include "pipeline/core/error_code.h"
#include "pipeline/core/filter_callback.h"
#include "pipeline/core/pipeline.h"
#include "pipeline/core/pipeline_core.h"
#include "pipeline/filters/codec/audio_decoder/audio_decoder_filter.h"
#include "pipeline/filters/sink/audio_sink/audio_sink_filter.h"
#include "play_executor.h"
#include "scene/lite/hiplayer.h"

namespace OHOS {
namespace Media {
class HiPlayerImpl : public Pipeline::EventReceiver,
                     public PlayExecutor,
                     public StateChangeCallback,
                     public Pipeline::FilterCallback,
                     public PlayerInterface {
    friend class StateMachine;

public:
    ~HiPlayerImpl() override;

    static std::shared_ptr<HiPlayerImpl> CreateHiPlayerImpl();

    // interface from PlayerInterface
    int32_t Init() override;
    int32_t DeInit() override;
    int32_t SetSource(const Source& source) override;
    int32_t Prepare() override;
    int32_t Play() override;
    bool IsPlaying() override;
    int32_t Pause() override;
    int32_t Stop() override;
    int32_t Reset() override;
    int32_t Release() override;
    int32_t Rewind(int64_t mSeconds, int32_t mode) override;
    int32_t SetVolume(float leftVolume, float rightVolume) override;
#ifndef SURFACE_DISABLED
    int32_t SetSurface(Surface* surface) override;
#endif
    bool IsSingleLooping() override;
    int32_t SetLoop(bool loop) override;
    void SetPlayerCallback(const std::shared_ptr<PlayerCallback>& cb) override;
    int32_t GetPlayerState(int32_t& state) override;
    int32_t GetCurrentPosition(int64_t& currentPositionMs) override;
    int32_t GetDuration(int64_t& outDurationMs) override;
    int32_t GetVideoWidth(int32_t& videoWidth) override
    {
        return CppExt::to_underlying(ErrorCode::ERROR_UNIMPLEMENTED);
    }
    int32_t GetVideoHeight(int32_t& videoHeight) override
    {
        return CppExt::to_underlying(ErrorCode::ERROR_UNIMPLEMENTED);
    }
    int32_t SetPlaybackSpeed(float speed) override
    {
        return CppExt::to_underlying(ErrorCode::ERROR_UNIMPLEMENTED);
    }
    int32_t GetPlaybackSpeed(float& speed) override
    {
        return CppExt::to_underlying(ErrorCode::ERROR_UNIMPLEMENTED);
    }
    int32_t SetAudioStreamType(int32_t type) override
    {
        return CppExt::to_underlying(ErrorCode::ERROR_UNIMPLEMENTED);
    }
    void GetAudioStreamType(int32_t& type) override
    {
        type = -1;
    }

    int32_t SetParameter(const Format& params) override
    {
        return CppExt::to_underlying(ErrorCode::ERROR_UNIMPLEMENTED);
    }

    void OnEvent(const Event& event) override;

    ErrorCode Resume();

    ErrorCode SetBufferSize(size_t size);

    ErrorCode GetSourceMeta(std::shared_ptr<const Plugin::Meta>& meta) const;
    ErrorCode GetTrackCnt(size_t& cnt) const;
    ErrorCode GetTrackMeta(size_t id, std::shared_ptr<const Plugin::Meta>& meta) const;

    ErrorCode SetVolume(float volume);

    void OnStateChanged(StateId state) override;

    ErrorCode OnCallback(const Pipeline::FilterCallbackType& type, Pipeline::Filter* filter,
                         const Plugin::Any& parameter) override;

    // interface from PlayExecutor
    ErrorCode DoSetSource(const std::shared_ptr<MediaSource>& source) const override;
    ErrorCode PrepareFilters() override;
    ErrorCode DoPlay() override;
    ErrorCode DoPause() override;
    ErrorCode DoResume() override;
    ErrorCode DoStop() override;
    ErrorCode DoSeek(bool allowed, int64_t hstTime, Plugin::SeekMode mode) override;
    ErrorCode DoOnReady() override;
    ErrorCode DoOnComplete() override;
    ErrorCode DoOnError(ErrorCode) override;

private:
    enum class MediaType : int32_t { AUDIO, VIDEO, BUTT };

    static Plugin::SeekMode Transform2SeekMode(PlayerSeekMode mode);

    struct MediaStat {
        MediaType mediaType {MediaType::BUTT};
        std::atomic<int64_t> currentPositionMs {0};
        std::atomic<bool> completeEventReceived {false};
        explicit MediaStat(MediaType mediaType) : mediaType(mediaType)
        {
        }
        MediaStat(const MediaStat& other) : mediaType(other.mediaType)
        {
            currentPositionMs = other.currentPositionMs.load();
            completeEventReceived = other.completeEventReceived.load();
        }
        MediaStat& operator=(const MediaStat& other)
        {
            currentPositionMs = other.currentPositionMs.load();
            completeEventReceived = other.completeEventReceived.load();
            return *this;
        }
    };

    class MediaStats {
    public:
        MediaStats() = default;
        void Reset();
        void Append(MediaType mediaType);
        void ReceiveEvent(EventType eventType, int64_t param = 0);
        int64_t GetCurrentPosition();
        bool IsEventCompleteAllReceived();

    private:
        std::vector<MediaStat> mediaStats;
    };

    HiPlayerImpl();
    HiPlayerImpl(const HiPlayerImpl& other);
    HiPlayerImpl& operator=(const HiPlayerImpl& other);
    ErrorCode StopAsync();

    Pipeline::PFilter CreateAudioDecoder(const std::string& desc);

    ErrorCode NewAudioPortFound(Pipeline::Filter* filter, const Plugin::Any& parameter);
#ifdef VIDEO_SUPPORT
    ErrorCode NewVideoPortFound(Pipeline::Filter* filter, const Plugin::Any& parameter);
#endif

    ErrorCode RemoveFilterChains(Pipeline::Filter* filter, const Plugin::Any& parameter);

    void ActiveFilters(const std::vector<Pipeline::Filter*>& filters);

    OSAL::Mutex stateMutex_;
    OSAL::ConditionVariable cond_;
    StateMachine fsm_;
    std::atomic<StateId> curFsmState_;

    std::shared_ptr<Pipeline::PipelineCore> pipeline_;
    std::atomic<PlayerStates> pipelineStates_;
    std::atomic<bool> initialized_ {false};

    std::shared_ptr<Pipeline::MediaSourceFilter> audioSource_;

    std::shared_ptr<Pipeline::DemuxerFilter> demuxer_;
    std::shared_ptr<Pipeline::AudioDecoderFilter> audioDecoder_;
    std::shared_ptr<Pipeline::AudioSinkFilter> audioSink_;
#ifdef VIDEO_SUPPORT
    std::shared_ptr<Pipeline::VideoDecoderFilter> videoDecoder;
    std::shared_ptr<Pipeline::VideoSinkFilter> videoSink;
#endif

    std::unordered_map<std::string, std::shared_ptr<Pipeline::AudioDecoderFilter>> audioDecoderMap_;

    std::weak_ptr<Plugin::Meta> sourceMeta_;
    std::vector<std::weak_ptr<Plugin::Meta>> streamMeta_;
    std::atomic<bool> singleLoop_ {false};

    std::weak_ptr<PlayerCallback> callback_;
    float volume_;
    std::atomic<ErrorCode> errorCode_;
    MediaStats mediaStats_;
};
} // namespace Media
} // namespace OHOS
#endif
