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
#define HST_LOG_TAG "HttpsCurlClient"
#include "https_curl_client.h"

#include <utility>
#include "foundation/log.h"

namespace OHOS {
namespace Media {
namespace Plugin {
namespace HttpPlugin {
HttpsCurlClient::HttpsCurlClient(std::shared_ptr<NetworkClient> httpClient) : httpClient_(std::move(httpClient))
{
}

HttpsCurlClient::~HttpsCurlClient() = default;

Status HttpsCurlClient::Init()
{
    // Do more https init things
    return Status::OK; // httpClient_->Init(); avoid re init http client
}

Status HttpsCurlClient::Open(const std::string& url)
{
    // Do more https open things
    return httpClient_->Open(url);
}

Status HttpsCurlClient::Close()
{
    // Do more https close things
    return httpClient_->Close();
}

Status HttpsCurlClient::Deinit()
{
    // Do more https Deinit things
    return Status::OK; // httpClient_->Deinit();  avoid re deinit http client
}

Status HttpsCurlClient::RequestData(long startPos, int len, NetworkServerErrorCode& serverCode,
                                    NetworkClientErrorCode& clientCode)
{
    // Do more https RequestData things
    return httpClient_->RequestData(startPos, len, serverCode, clientCode);
}
} // HttpPlugin
} // Plugin
} // Media
} // OHOS