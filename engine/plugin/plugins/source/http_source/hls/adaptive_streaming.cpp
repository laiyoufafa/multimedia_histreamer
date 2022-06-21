/*
 * Copyright (c) 2022-2022 Huawei Device Co., Ltd.
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
#define HST_LOG_TAG "AdaptiveStreaming"
#include "adaptive_streaming.h"
#include "securec.h"

namespace OHOS {
namespace Media {
namespace Plugin {
namespace HttpPlugin {
AdaptiveStreaming::AdaptiveStreaming()
{
    downloader = std::make_shared<Downloader>("hlsPlayList");
    dataSave_ = [this] (uint8_t*&& data, uint32_t&& len, int64_t&& offset) {
        SaveData(std::forward<decltype(data)>(data), std::forward<decltype(len)>(len),
                 std::forward<decltype(offset)>(offset));
    };
    // this is default callback
    statusCallback_ = [this] (DownloadStatus&& status, std::shared_ptr<Downloader> d,
            std::shared_ptr<DownloadRequest>& request) {
        OnDownloadStatus(std::forward<decltype(status)>(status), downloader,
            std::forward<decltype(request)>(request));
    };
    updateTask_ = std::make_shared<OSAL::Task>(std::string("FragmentListUpdate"));
    updateTask_->RegisterHandler([this] { FragmentListUpdateLoop(); });
}

AdaptiveStreaming::~AdaptiveStreaming()
{
    downloader->Stop();
    updateTask_->Stop();
}

void AdaptiveStreaming::DoOpen(const std::string& url)
{
    playList_.clear();
    auto realStatusCallback = [this] (DownloadStatus&& status, std::shared_ptr<Downloader>& downloader,
                                      std::shared_ptr<DownloadRequest>& request) {
        statusCallback_(status, downloader, std::forward<decltype(request)>(request));
    };
    downloadRequest_ = std::make_shared<DownloadRequest>(url, dataSave_, realStatusCallback, true);
    downloader->Download(downloadRequest_, -1); // -1
    downloader->Start();
}

void AdaptiveStreaming::SaveData(uint8_t* data, uint32_t len, int64_t offset)
{
    (void)offset;
    playList_.append(reinterpret_cast<const char*>(data), len);
    ParseManifest();
}

void AdaptiveStreaming::OnDownloadStatus(DownloadStatus status, std::shared_ptr<Downloader>&,
                                         std::shared_ptr<DownloadRequest>& request)
{
    // This should not be called normally
    MEDIA_LOG_D("Should not call this OnDownloadStatus, should call monitor.");
    if (request->GetClientError() != NetworkClientErrorCode::ERROR_OK || request->GetServerError() != 0) {
        MEDIA_LOG_E("OnDownloadStatus " PUBLIC_LOG_D32 ", url : " PUBLIC_LOG_S,
                    status, request->GetUrl().c_str());
    }
}

void AdaptiveStreaming::SetStatusCallback(StatusCallbackFunc cb)
{
    statusCallback_ = cb;
}

void AdaptiveStreaming::ParseManifest()
{
    MEDIA_LOG_E("Should not call this ParseManifest");
}

void AdaptiveStreaming::Resume()
{
    downloader->Resume();
    updateTask_->Start();
}

void AdaptiveStreaming::Pause()
{
    downloader->Pause();
    updateTask_->Pause();
}

void AdaptiveStreaming::Close()
{
    downloader->Stop();
    updateTask_->Stop();
}
}
}
}
}