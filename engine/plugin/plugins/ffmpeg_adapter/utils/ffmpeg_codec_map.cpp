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

#include "ffmpeg_codec_map.h"
#include "utils/constants.h"

namespace OHOS {
namespace Media {
namespace FFCodecMap {
using namespace OHOS::Media;

bool CodecId2Cap(AVCodecID codecId, bool encoder, Plugin::Capability& cap)
{
    switch (codecId) {
        case AV_CODEC_ID_MP3:
            cap.SetMime(OHOS::Media::MEDIA_MIME_AUDIO_MPEG)
                .AppendFixedKey<uint32_t>(Plugin::Capability::Key::AUDIO_MPEG_VERSION, 1)
                .AppendIntervalKey<uint32_t>(Plugin::Capability::Key::AUDIO_MPEG_LAYER, 1, 3); // 3
            return true;
        case AV_CODEC_ID_FLAC:
            cap.SetMime(OHOS::Media::MEDIA_MIME_AUDIO_FLAC);
            return true;
        case AV_CODEC_ID_AAC:
            cap.SetMime(OHOS::Media::MEDIA_MIME_AUDIO_AAC);
            return true;
        case AV_CODEC_ID_AAC_LATM:
            cap.SetMime(OHOS::Media::MEDIA_MIME_AUDIO_AAC_LATM);
            return true;
        default:
            break;
    }
    return false;
}
bool FormatName2Cap(const std::string& fmtName, Plugin::CapabilitySet& outCaps)
{
    if (fmtName == "mp4") {
        outCaps.emplace_back(Plugin::Capability(MEDIA_MIME_CONTAINER_MP4));
        return true;
    }
    return false;
}
bool Mime2CodecId(const std::string& mime, AVCodecID& codecId)
{
    if (mime == MEDIA_MIME_AUDIO_AAC) {
        codecId = AV_CODEC_ID_AAC;
        return true;
    }
    return false;
}
} // FFCodecMap
} // Media
} // OHOS