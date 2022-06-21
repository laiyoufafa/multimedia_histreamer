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
#define HST_LOG_TAG "HlsMediaDownloader"

#include "hls_media_downloader.h"
#include "hls_streaming.h"
#include "securec.h"

namespace OHOS {
namespace Media {
namespace Plugin {
namespace HttpPlugin {
namespace {
constexpr int RING_BUFFER_SIZE = 5 * 48 * 1024;
}

HlsMediaDownloader::HlsMediaDownloader() noexcept
{
    buffer_ = std::make_shared<RingBuffer>(RING_BUFFER_SIZE);
    buffer_->Init();

    downloader_ = std::make_shared<Downloader>("hlsMedia");
    downloadTask_ = std::make_shared<OSAL::Task>(std::string("FragmentDownload"));
    downloadTask_->RegisterHandler([this] { FragmentDownloadLoop(); });

    fragmentList_ = std::make_shared<BlockingQueue<std::string>>("FragmentList", 50); // 50

    dataSave_ =  [this] (uint8_t*&& data, uint32_t&& len, int64_t&& offset) {
        SaveData(std::forward<decltype(data)>(data), std::forward<decltype(len)>(len),
                 std::forward<decltype(offset)>(offset));
    };

    adaptiveStreaming_ = std::make_shared<HLSStreaming>();
    adaptiveStreaming_->SetFragmentListCallback(this);
}

void HlsMediaDownloader::FragmentDownloadLoop()
{
    std::string url = fragmentList_->Pop();
    if (!fragmentDownloadStart[url]) {
        fragmentDownloadStart[url] = true;
        auto realStatusCallback = [this] (DownloadStatus&& status, std::shared_ptr<Downloader>& downloader,
                                          std::shared_ptr<DownloadRequest>& request) {
            statusCallback_(status, downloader_, std::forward<decltype(request)>(request));
        };
        // TO DO: If the fragment file is too large, should not requestWholeFile.
        downloadRequest_ = std::make_shared<DownloadRequest>(url, dataSave_, realStatusCallback, true);
        downloader_->Download(downloadRequest_, -1); // -1
        downloader_->Start();
    }
}

bool HlsMediaDownloader::Open(const std::string& url)
{
    adaptiveStreaming_->Open(url);
    downloadTask_->Start();
    return true;
}

void HlsMediaDownloader::Close()
{
    buffer_->SetActive(false);
    fragmentList_->SetActive(false);
    adaptiveStreaming_->Close();
    downloadTask_->Stop();
    downloader_->Stop();
}

void HlsMediaDownloader::Pause()
{
    buffer_->SetActive(false);
    fragmentList_->SetActive(false);
    adaptiveStreaming_->Pause();
    downloadTask_->Pause();
    downloader_->Pause();
}

void HlsMediaDownloader::Resume()
{
    buffer_->SetActive(true);
    fragmentList_->SetActive(true);
    adaptiveStreaming_->Resume();
    downloadTask_->Start();
    downloader_->Resume();
}

bool HlsMediaDownloader::Read(unsigned char* buff, unsigned int wantReadLength,
                              unsigned int& realReadLength, bool& isEos)
{
    FALSE_RETURN_V(buffer_ != nullptr, false);
    realReadLength = buffer_->ReadBuffer(buff, wantReadLength, 2); // wait 2 times
    MEDIA_LOG_D("Read: wantReadLength " PUBLIC_LOG_D32 ", realReadLength " PUBLIC_LOG_D32 ", isEos "
                PUBLIC_LOG_D32, wantReadLength, realReadLength, isEos);
    return true;
}

bool HlsMediaDownloader::Seek(int offset)
{
    FALSE_RETURN_V(buffer_ != nullptr, false);
    MEDIA_LOG_I("Seek: buffer size " PUBLIC_LOG_ZU ", offset " PUBLIC_LOG_D32, buffer_->GetSize(), offset);
    if (buffer_->Seek(offset)) {
        return true;
    }
    buffer_->Clear(); // First clear buffer, avoid no available buffer then task pause never exit.
    downloader_->Pause();
    buffer_->Clear();
    downloader_->Seek(offset);
    downloader_->Start();
    return true;
}

size_t HlsMediaDownloader::GetContentLength() const
{
    return 0;
}

double HlsMediaDownloader::GetDuration() const
{
    return adaptiveStreaming_->GetDuration();
}

Seekable HlsMediaDownloader::GetSeekable() const
{
    return adaptiveStreaming_->GetSeekable();
}

void HlsMediaDownloader::SetCallback(Callback* cb)
{
    callback_ = cb;
}

void HlsMediaDownloader::OnFragmentListChanged(const std::vector<std::string>& fragmentList)
{
    for (auto& fragment : fragmentList) {
        fragmentList_->Push(fragment);
    }
}

void HlsMediaDownloader::SaveData(uint8_t* data, uint32_t len, int64_t offset)
{
    buffer_->WriteBuffer(data, len, offset);
}

void HlsMediaDownloader::SetStatusCallback(StatusCallbackFunc cb)
{
    statusCallback_ = cb;
    adaptiveStreaming_->SetStatusCallback(cb);
}
}
}
}
}