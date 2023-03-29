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


#include "plugin/core/plugin_wrapper.h"
#include <set>

namespace {
std::set<OHOS::Media::Plugin::MetaID> g_metaIdSet = {
    OHOS::Media::Plugin::MetaID::MIME,
    OHOS::Media::Plugin::MetaID::TRACK_ID,
    OHOS::Media::Plugin::MetaID::MEDIA_CODEC_CONFIG,
    OHOS::Media::Plugin::MetaID::MEDIA_BITRATE,
    OHOS::Media::Plugin::MetaID::AUDIO_CHANNELS,
    OHOS::Media::Plugin::MetaID::AUDIO_SAMPLE_RATE,
    OHOS::Media::Plugin::MetaID::AUDIO_SAMPLE_FORMAT,
    OHOS::Media::Plugin::MetaID::AUDIO_SAMPLE_PER_FRAME,
    OHOS::Media::Plugin::MetaID::AUDIO_CHANNEL_LAYOUT,
    OHOS::Media::Plugin::MetaID::MEDIA_TITLE,
    OHOS::Media::Plugin::MetaID::MEDIA_ARTIST,
    OHOS::Media::Plugin::MetaID::MEDIA_LYRICIST,
    OHOS::Media::Plugin::MetaID::MEDIA_ALBUM,
    OHOS::Media::Plugin::MetaID::MEDIA_ALBUM_ARTIST,
    OHOS::Media::Plugin::MetaID::MEDIA_DATE,
    OHOS::Media::Plugin::MetaID::MEDIA_COMMENT,
    OHOS::Media::Plugin::MetaID::MEDIA_GENRE,
    OHOS::Media::Plugin::MetaID::MEDIA_DESCRIPTION,
    OHOS::Media::Plugin::MetaID::MEDIA_COPYRIGHT,
    OHOS::Media::Plugin::MetaID::MEDIA_LANGUAGE,
    OHOS::Media::Plugin::MetaID::MEDIA_LYRICS,
    OHOS::Media::Plugin::MetaID::MEDIA_DURATION,
    OHOS::Media::Plugin::MetaID::MEDIA_FILE_URI,
    OHOS::Media::Plugin::MetaID::MEDIA_FILE_SIZE,
    OHOS::Media::Plugin::MetaID::MEDIA_SEEKABLE,
    OHOS::Media::Plugin::MetaID::MEDIA_TYPE,
    OHOS::Media::Plugin::MetaID::AUDIO_MPEG_VERSION,
    OHOS::Media::Plugin::MetaID::AUDIO_MPEG_LAYER,
    OHOS::Media::Plugin::MetaID::AUDIO_AAC_PROFILE,
    OHOS::Media::Plugin::MetaID::AUDIO_AAC_LEVEL,
    OHOS::Media::Plugin::MetaID::AUDIO_AAC_STREAM_FORMAT,
    OHOS::Media::Plugin::MetaID::VIDEO_WIDTH,
    OHOS::Media::Plugin::MetaID::VIDEO_HEIGHT,
    OHOS::Media::Plugin::MetaID::VIDEO_PIXEL_FORMAT,
    OHOS::Media::Plugin::MetaID::VIDEO_BIT_STREAM_FORMAT,
    OHOS::Media::Plugin::MetaID::VIDEO_FRAME_RATE,
    OHOS::Media::Plugin::MetaID::VIDEO_H264_LEVEL,
    OHOS::Media::Plugin::MetaID::VIDEO_H264_PROFILE,
    OHOS::Media::Plugin::MetaID::BITS_PER_CODED_SAMPLE,
};
}

namespace OHOS {
namespace Media {
namespace Plugin {
DataSourceWrapper::DataSourceWrapper(uint32_t pkgVersion, std::shared_ptr<DataSourceHelper> dataSource)
    : version(pkgVersion), helper(std::move(dataSource))
{
}

Status DataSourceWrapper::ReadAt(int64_t offset, std::shared_ptr<Buffer>& buffer, size_t expectedLen)
{
    return helper->ReadAt(offset, buffer, expectedLen);
}

Status DataSourceWrapper::GetSize(size_t& size)
{
    return helper->GetSize(size);
}

Seekable DataSourceWrapper::GetSeekable()
{
    return helper->GetSeekable();
}

DataSinkWrapper::DataSinkWrapper(uint32_t pkgVersion, std::shared_ptr<DataSinkHelper> dataSink)
    : version(pkgVersion), helper(std::move(dataSink)) {}

Status DataSinkWrapper::WriteAt(int64_t offset, const std::shared_ptr<Buffer>& buffer)
{
    return helper->WriteAt(offset, buffer);
}

void ConvertToMediaInfoHelper(uint32_t pkgVersion, const MediaInfo& src, MediaInfoHelper& dest)
{
    (void)(pkgVersion);
    for (auto const& global : src.general) {
        dest.globalMeta.SetData(MetaID(global.first), global.second);
    }
    size_t streamSize = src.tracks.size();
    if (streamSize <= 0) {
        return;
    }
    dest.trackMeta.resize(streamSize);
    for (size_t i = 0; i < streamSize; ++i) {
        for (auto const& meta : src.tracks[i]) {
            if (g_metaIdSet.count(MetaID(meta.first))) {
                dest.trackMeta[i].SetData(MetaID(meta.first), meta.second);
            }
        }
    }
}
} // namespace Plugin
} // namespace Media
} // namespace OHOS
