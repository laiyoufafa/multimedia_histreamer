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

#define HST_LOG_TAG "FFMpeg_Muxer"

#include "ffmpeg_muxer_plugin.h"

#include <functional>
#include <set>

#include "foundation/log.h"
#include "plugin/interface/plugin_definition.h"
#include "plugin/plugins/ffmpeg_adapter/utils/ffmpeg_utils.h"
#include "plugin/plugins/ffmpeg_adapter/utils/ffmpeg_codec_map.h"

namespace {
using namespace OHOS::Media;

std::map<std::string, std::shared_ptr<AVOutputFormat>> g_pluginOutputFmt;

std::set<std::string> g_supportedMuxer = {"mp4"};

bool IsMuxerSupported(const char* name)
{
    return g_supportedMuxer.count(name) == 1;
}

bool UpdatePluginInCapability(AVCodecID codecId, CapabilitySet& capSet)
{
    if (codecId != AV_CODEC_ID_NONE) {
        Capability cap;
        if (!FFCodecMap::CodecId2Cap(codecId, true, cap)) {
            return false;
        } else {
            capSet.emplace_back(cap);
        }
    }
    return true;
}

bool UpdatePluginCapability(const AVOutputFormat* oFmt, Plugin::MuxerPluginDef& pluginDef)
{
    if (!FFCodecMap::FormatName2Cap(oFmt->name, pluginDef.outCaps)) {
        MEDIA_LOG_D("%s is not supported now", oFmt->name);
        return false;
    }
    UpdatePluginInCapability(oFmt->audio_codec, pluginDef.inCaps);
    UpdatePluginInCapability(oFmt->video_codec, pluginDef.inCaps);
    UpdatePluginInCapability(oFmt->subtitle_codec, pluginDef.inCaps);
    return true;
}

Plugin::Status RegisterMuxerPlugins(const std::shared_ptr<Plugin::Register>& reg)
{
    MEDIA_LOG_D("register muxer plugins.");
    if (!reg) {
        MEDIA_LOG_E("RegisterPlugins failed due to null pointer for reg.");
        return Plugin::Status::ERROR_INVALID_PARAMETER;
    }
    const AVOutputFormat* outputFormat = nullptr;
    void* ite = nullptr;
    while ((outputFormat = av_muxer_iterate(&ite))) {
        MEDIA_LOG_D("check ffmpeg muxer %s", outputFormat->name);
        if (!IsMuxerSupported(outputFormat->name)) {
            continue;
        }
        if (outputFormat->long_name != nullptr) {
            if (!strncmp(outputFormat->long_name, "raw ", 4)) {
                continue;
            }
        }
        std::string pluginName = "ffmpegMux_" + std::string(outputFormat->name);
        Plugin::ReplaceDelimiter(".,|-<> ", '_', pluginName);
        Plugin::MuxerPluginDef def;
        if (!UpdatePluginCapability(outputFormat, def)) {
            continue;
        }
        def.name = pluginName;
        def.description = "ffmpeg muxer";
        def.rank = 100; // 100
        def.creator = [](const std::string& name) -> std::shared_ptr<Plugin::MuxerPlugin>{
            return std::make_shared<FFMux::FFmpegMuxerPlugin>(name);
        };
        if (reg->AddPlugin(def) != Plugin::Status::OK) {
            MEDIA_LOG_W("fail to add plugin %s", pluginName.c_str());
            continue;
        }
        g_pluginOutputFmt[pluginName] = std::shared_ptr<AVOutputFormat>(const_cast<AVOutputFormat*>(outputFormat),
                                                                        [](AVOutputFormat* ptr){}); // do not delete
    }
}
PLUGIN_DEFINITION(FFMpegMuxers, Plugin::LicenseType::LGPL, RegisterMuxerPlugins, []{g_pluginOutputFmt.clear();})

Plugin::Status SetCodecByMime(const AVOutputFormat* fmt, const std::string& mime, AVStream* stream)
{
    AVCodecID id = AV_CODEC_ID_NONE;
    if (!FFCodecMap::Mime2CodecId(mime, id)) {
        MEDIA_LOG_E("mime %s has no corresponding codec id", mime.c_str());
        return Plugin::Status::ERROR_UNSUPPORTED_FORMAT;
    }
    auto ptr = avcodec_find_encoder(id);
    if (ptr == nullptr) {
        MEDIA_LOG_E("codec of mime %s is not founder as encoder", mime.c_str());
        return Plugin::Status::ERROR_UNSUPPORTED_FORMAT;
    }
    bool matched = true;
    switch (ptr->type) {
        case AVMEDIA_TYPE_VIDEO:
            matched = id == fmt->video_codec;
            break;
        case AVMEDIA_TYPE_AUDIO:
            matched = id == fmt->audio_codec;
            break;
        case AVMEDIA_TYPE_SUBTITLE:
            matched = id == fmt->subtitle_codec;
            break;
        default:
            matched = false;
    }
    if (!matched) {
        MEDIA_LOG_E("codec of mime %s is not matched with %s muxer", mime.c_str(), fmt->name);
        return Plugin::Status::ERROR_UNSUPPORTED_FORMAT;
    }
    stream->codecpar->codec_id = id;
    stream->codecpar->codec_type = ptr->type;
    return Plugin::Status::OK;
}

Plugin::Status SetCodecOfTrack(const AVOutputFormat* fmt, AVStream* stream, const Plugin::TagMap& tagMap)
{
    using namespace OHOS::Media::Plugin;
    if (tagMap.count(Tag::MIME) == 0) {
        MEDIA_LOG_E("mime is missing!");
        return Status::ERROR_UNSUPPORTED_FORMAT;
    }
    auto& value = tagMap.find(Tag::MIME)->second;
    if (value.Type() != typeid(std::string)) {
        return Status::ERROR_MISMATCHED_TYPE;
    }
    // todo specially for audio/mpeg audio/mpeg we should check mpegversion and mpeglayer

    return SetCodecByMime(fmt, Plugin::AnyCast<std::string>(value), stream);
}

template<typename T, typename U>
Plugin::Status SetSingleParameter(Tag tag, const Plugin::TagMap& tagMap, U& target, std::function<U(T)> func)
{
    auto ite = tagMap.find(tag);
    if (ite != std::end(tagMap)) {
        if (ite->second.Type() != typeid(T)) {
            MEDIA_LOG_E("tag %d type mismatched", tag);
            return Plugin::Status::ERROR_MISMATCHED_TYPE;
        }
        target = func(Plugin::AnyCast<T>(ite->second));
    }
    return Plugin::Status::OK;
}

Plugin::Status SetParameterOfAuTrack(AVStream* stream, const Plugin::TagMap& tagMap)
{
#define RETURN_IF_NOT_OK(ret) \
do { \
    if ((ret) != Status::OK) { \
        return ret; \
    } \
} while (0)

    using namespace Plugin;
    auto ret = SetSingleParameter<AudioSampleFormat, int32_t>(Tag::AUDIO_SAMPLE_FORMAT, tagMap,
                                                              stream->codecpar->format, Trans2FFmepgFormat);
    std::function<int32_t(uint32_t)> ui2iFunc = [](uint32_t i){return i;};

    RETURN_IF_NOT_OK(ret);
    ret = SetSingleParameter<uint32_t, int32_t>(Tag::AUDIO_CHANNELS, tagMap, stream->codecpar->channels,
                                                ui2iFunc);
    RETURN_IF_NOT_OK(ret);
    ret = SetSingleParameter<uint32_t, int32_t>(Tag::AUDIO_SAMPLE_RATE, tagMap, stream->codecpar->sample_rate,
                                                ui2iFunc);
    RETURN_IF_NOT_OK(ret);
    ret = SetSingleParameter<AudioChannelLayout, uint64_t>(Tag::AUDIO_CHANNEL_LAYOUT, tagMap,
        stream->codecpar->channel_layout, [](AudioChannelLayout layout){return (uint64_t)layout;});
    // specially for some format
//    if (stream->codecpar->codec_id == AV_CODEC_ID_AAC || stream->codecpar->codec_id == AV_CODEC_ID_AAC_LATM) {
//        ret = SetSingleParameter<AudioAacProfile, int32_t>(Tag::AUDIO_AAC_PROFILE, tagMap, stream->codecpar->profile,
//                                                           []());
//    }

    return ret;
#undef RETURN_IF_NOT_OK
}

Plugin::Status SetParameterOfVdTrack(AVStream* stream, const Plugin::TagMap& tagMap)
{
    // todo add video
    MEDIA_LOG_E("should add vd tack parameter setter");
    return Plugin::Status::ERROR_UNKNOWN;
}

Plugin::Status SetParameterOfSubTitleTrack(AVStream* stream, const Plugin::TagMap& tagMap)
{
    // todo add subtitle
    MEDIA_LOG_E("should add subtitle tack parameter setter");
    return Plugin::Status::ERROR_UNKNOWN;
}

Plugin::Status SetTagsOfTrack(const AVOutputFormat* fmt, AVStream* stream, const Plugin::TagMap& tagMap)
{
    using namespace OHOS::Media::Plugin;
    if (stream == nullptr) {
        return Status::ERROR_INVALID_PARAMETER;
    }
    // firstly mime
    auto ret = SetCodecOfTrack(fmt, stream, tagMap);
    if (ret != Status::OK) {
        return ret;
    }

    if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) { // audio
        return SetParameterOfAuTrack(stream, tagMap);
    } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) { // video
        return SetParameterOfVdTrack(stream, tagMap);
    } else if (stream->codecpar->codec_type == AVMEDIA_TYPE_SUBTITLE) { // subtitle
        return SetParameterOfSubTitleTrack(stream, tagMap);
    } else {
        MEDIA_LOG_W("unknown codec type of stream %d", stream->index);
    }
    // others
    ret = SetSingleParameter<int64_t, int64_t>(Tag::MEDIA_BITRATE, tagMap, stream->codecpar->bit_rate,
                                         [](int64_t rate){return rate;});
    if (ret != Status::OK) {
        return ret;
    }

    // extra data
    auto ite = tagMap.find(Tag::MEDIA_CODEC_CONFIG);
    if (ite != std::end(tagMap)) {
        if (ite->second.Type() != typeid(std::vector<uint8_t>)) {
            MEDIA_LOG_E("tag %d type mismatched", Tag::MEDIA_CODEC_CONFIG);
            return Plugin::Status::ERROR_MISMATCHED_TYPE;
        }
        auto extraData = Plugin::AnyCast<std::vector<uint8_t>>(ite->second);
        stream->codecpar->extradata = static_cast<uint8_t *>(av_mallocz(
                extraData.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        if (stream->codecpar->extradata == nullptr) {
            return Status::ERROR_NO_MEMORY;
        }
        memcpy_s(stream->codecpar->extradata, extraData.size(), extraData.data(), extraData.size());
        stream->codecpar->extradata_size = extraData.size();
    }
    return Status::OK;
}

Plugin::Status SetTagsOfGeneral(AVFormatContext* fmtCtx, const Plugin::TagMap& tags)
{
    for (const auto& pair: tags) {
        std::string metaName;
        if (!Plugin::FindAvMetaNameByTag(pair.first, metaName)) {
            MEDIA_LOG_I("tag %d will not written as general meta", pair.first);
            continue;
        }
        if (pair.second.Type() != typeid(std::string)) {
            continue;
        }
        std::string value = Plugin::AnyCast<std::string>(pair.second);
        av_dict_set(&fmtCtx->metadata, metaName.c_str(), value.c_str(), 0);
    }
    return Plugin::Status::OK;
}
}

namespace OHOS{
namespace Media {
namespace FFMux {
using namespace OHOS::Media::Plugin;

FFmpegMuxerPlugin::FFmpegMuxerPlugin(std::string name) : MuxerPlugin(std::move(name)) {}

FFmpegMuxerPlugin::~FFmpegMuxerPlugin()
{
    Release();
}
Status FFmpegMuxerPlugin::Release()
{
    formatContext_.reset();
    outputFormat_.reset();
}

Status FFmpegMuxerPlugin::InitFormatCtxLocked()
{
    if (formatContext_ == nullptr) {
        auto ioCtx = InitAvIoCtx();
        if (ioCtx == nullptr) {
            return Status::ERROR_NO_MEMORY;
        }
        auto fmt = avformat_alloc_context();
        if (fmt == nullptr) {
            return Status::ERROR_NO_MEMORY;
        }
        fmt->pb = ioCtx;
        fmt->flags = static_cast<uint32_t>(fmt->flags) | static_cast<uint32_t>(AVFMT_FLAG_CUSTOM_IO);
        formatContext_ = std::shared_ptr<AVFormatContext>(fmt, [](AVFormatContext* ptr) {
            if (ptr) {
                DeInitAvIoCtx(ptr->pb);
                avformat_free_context(ptr);
            }
        });
    }
    return Status::OK;
}

Status FFmpegMuxerPlugin::Init()
{
    MEDIA_LOG_D("Init entered.");
    if (g_pluginOutputFmt.count(pluginName_) == 0) {
        return Status::ERROR_UNSUPPORTED_FORMAT;
    }
    outputFormat_ = g_pluginOutputFmt[pluginName_];
    OSAL::ScopedLock lock(fmtMutex_);
    return InitFormatCtxLocked();
}

Status FFmpegMuxerPlugin::Deinit()
{
    return Release();
}

AVIOContext* FFmpegMuxerPlugin::InitAvIoCtx()
{
    constexpr int bufferSize = 4096;
    auto buffer = static_cast<unsigned char*>(av_malloc(bufferSize));
    if (buffer == nullptr) {
        MEDIA_LOG_E("AllocAVIOContext failed to av_malloc...");
        return nullptr;
    }
    AVIOContext* avioContext = avio_alloc_context(buffer, bufferSize, AVIO_FLAG_WRITE, static_cast<void*>(&ioContext_),
                                                  IoRead, IoWrite, IoSeek);
    if (avioContext == nullptr) {
        MEDIA_LOG_E("AllocAVIOContext failed to avio_alloc_context...");
        av_free(buffer);
        return nullptr;
    }
    avioContext->seekable = AVIO_SEEKABLE_NORMAL;
    return avioContext;
}

void FFmpegMuxerPlugin::DeInitAvIoCtx(AVIOContext* ptr)
{
    if (ptr != nullptr) {
        ptr->opaque = nullptr;
        av_freep(&ptr->buffer);
        avio_context_free(&ptr);
    }
}

Status FFmpegMuxerPlugin::Prepare()
{
    for (const auto& pair: trackParameters_) {
        SetTagsOfTrack(outputFormat_.get(), formatContext_->streams[pair.first], pair.second);
    }
    SetTagsOfGeneral(formatContext_.get(), generalParameters_);
    return Status::OK;
}
void FFmpegMuxerPlugin::ResetIoCtx(IOContext& ioContext)
{
    ioContext.dataSink_.reset();
    ioContext.pos_ = 0;
    ioContext.end_ = 0;
}
Status FFmpegMuxerPlugin::Reset()
{
    ResetIoCtx(ioContext_);
    generalParameters_.clear();
    trackParameters_.clear();
    OSAL::ScopedLock lock(fmtMutex_);
    if (outputFormat_->deinit) {
        outputFormat_->deinit(formatContext_.get());
    }
    formatContext_.reset();
    return InitFormatCtxLocked();
}

Status FFmpegMuxerPlugin::GetParameter(Tag tag, ValueType &value)
{
    return PluginBase::GetParameter(tag, value);
}

Status FFmpegMuxerPlugin::GetTrackParameter(uint32_t trackId, Tag tag, Plugin::ValueType& value)
{
    return Status::ERROR_UNIMPLEMENTED;
}

Status FFmpegMuxerPlugin::SetParameter(Tag tag, const ValueType &value)
{
    generalParameters_[tag] = value;
    return Status::OK;
}

Status FFmpegMuxerPlugin::SetTrackParameter(uint32_t trackId, Tag tag, const Plugin::ValueType& value)
{
    if (trackId >= formatContext_->nb_streams) {
        return Status::ERROR_INVALID_PARAMETER;
    }
    if (trackParameters_.count(trackId) == 0) {
        trackParameters_.insert({trackId, Plugin::TagMap()});
    }
    trackParameters_[trackId][tag] = value;
    return Status::OK;
}

Status FFmpegMuxerPlugin::AddTrack(uint32_t &trackId)
{
    OSAL::ScopedLock lock(fmtMutex_);
    if (formatContext_ == nullptr) {
        return Status::ERROR_WRONG_STATE;
    }
    auto st = avformat_new_stream(formatContext_.get(), nullptr);
    if (st == nullptr) {
        return Status::ERROR_NO_MEMORY;
    }
    st->codecpar->codec_type = AVMEDIA_TYPE_UNKNOWN;
    st->codecpar->codec_id = AV_CODEC_ID_NONE;
    trackId = st->index;
    return Status::OK;
}

Status FFmpegMuxerPlugin::SetDataSink(const std::shared_ptr<DataSink> &dataSink)
{
    ioContext_.dataSink_ = dataSink;
    return Status::OK;
}

Status FFmpegMuxerPlugin::WriteHeader()
{
    if (ioContext_.dataSink_ == nullptr || outputFormat_ == nullptr) {
        return Status::ERROR_WRONG_STATE;
    }
    OSAL::ScopedLock lock(fmtMutex_);
    if (formatContext_ == nullptr) {
        return Status::ERROR_WRONG_STATE;
    }
    int ret = avformat_write_header(formatContext_.get(), nullptr);
    if (ret < 0) {
        MEDIA_LOG_E("failed to write header %s", AVStrError(ret).c_str());
        return Status::ERROR_UNKNOWN;
    }
    return Status::OK;
}

Status FFmpegMuxerPlugin::WriteFrame(const std::shared_ptr<Plugin::Buffer>& buffer)
{
    if (buffer == nullptr || buffer->IsEmpty()) {
        return Status::ERROR_INVALID_PARAMETER;
    }
    uint32_t trackId = buffer->trackID;
    if (trackId > formatContext_->nb_streams) {
        return Status::ERROR_INVALID_PARAMETER;
    }
    AVPacket pkt;
    auto memory = buffer->GetMemory();
    pkt.data = const_cast<uint8_t*>(memory->GetReadOnlyData());
    pkt.size = memory->GetSize();
    pkt.stream_index = trackId;
    pkt.pts = ConvertTimeToFFmpeg(buffer->pts, formatContext_->streams[trackId]->time_base);
    pkt.dts = pkt.pts;
    pkt.flags = 0;
    if (buffer->flag & BUFFER_FLAG_KEY_FRAME) {
        pkt.flags |= AV_PKT_FLAG_KEY;
    }
    pkt.duration = ConvertTimeToFFmpeg(buffer->duration, formatContext_->streams[trackId]->time_base);
    av_write_frame(formatContext_.get(), &pkt);
    return Status::OK;
}

Status FFmpegMuxerPlugin::WriteTrailer()
{
    if (ioContext_.dataSink_ == nullptr || outputFormat_ == nullptr) {
        return Status::ERROR_WRONG_STATE;
    }
    OSAL::ScopedLock lock(fmtMutex_);
    if (formatContext_ == nullptr) {
        return Status::ERROR_WRONG_STATE;
    }
    int ret = av_write_trailer(formatContext_.get());
    if (ret != 0) {
        MEDIA_LOG_E("failed to write trailer %s", AVStrError(ret).c_str());
    }
    avio_flush(formatContext_->pb);
    return Status::OK;
}

Status FFmpegMuxerPlugin::SetCallback(const std::shared_ptr<Callback> &cb)
{
    return Status::END_OF_STREAM;
}

std::shared_ptr<Allocator> FFmpegMuxerPlugin::GetAllocator()
{
    return {};
}

int32_t FFmpegMuxerPlugin::IoRead(void* opaque, uint8_t* buf, int bufSize)
{
    (void)opaque;
    (void)buf;
    (void)bufSize;
    return 0;
}
int32_t FFmpegMuxerPlugin::IoWrite(void* opaque, uint8_t* buf, int bufSize)
{
    auto ioCtx = static_cast<IOContext*>(opaque);
    if (ioCtx && ioCtx->dataSink_) {
        auto buffer = std::make_shared<Buffer>();
        auto bufferMem = buffer->WrapMemory(buf, bufSize, bufSize);
        auto res = ioCtx->dataSink_->WriteAt(ioCtx->pos_, buffer);
        if (res == Status::OK) {
            ioCtx->pos_ += bufferMem->GetSize();
            if (ioCtx->pos_ > ioCtx->end_) {
                ioCtx->end_ = ioCtx->pos_;
            }
            return bufferMem->GetSize();
        }
        return 0;
    }
    return -1;
}

int64_t FFmpegMuxerPlugin::IoSeek(void* opaque, int64_t offset, int whence)
{
    auto ioContext = static_cast<IOContext*>(opaque);
    uint64_t newPos = 0;
    switch (whence) {
        case SEEK_SET:
            newPos = static_cast<uint64_t>(offset);
            ioContext->pos_ = newPos;
            MEDIA_LOG_I("AVSeek whence: %d, pos = %" PRId64 ", newPos = %" PRIu64, whence, offset, newPos);
            break;
        case SEEK_CUR:
            newPos = ioContext->pos_ + offset;
            MEDIA_LOG_I("AVSeek whence: %d, pos = %" PRId64 ", newPos = %" PRIu64, whence, offset, newPos);
            break;
        case SEEK_END:
        case AVSEEK_SIZE:
            newPos = ioContext->end_ + offset;
            MEDIA_LOG_I("AVSeek seek end whence: %d, pos = %" PRId64, whence, offset);
            break;
        default:
            MEDIA_LOG_E("AVSeek unexpected whence: %d", whence);
            break;
    }
    if (whence != AVSEEK_SIZE) {
        ioContext->pos_ = newPos;
    }
    MEDIA_LOG_I("current offset: %" PRId64 ", new pos: %" PRIu64, ioContext->pos_, newPos);
    return newPos;
}
} // FFMux
} // Media
} // OHOS