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
#define HST_LOG_TAG "FileFdSinkPlugin"

#include "file_fd_sink_plugin.h"
#ifdef WIN32
#include <fcntl.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif
#include "foundation/log.h"

namespace OHOS {
namespace Media {
namespace Plugin {
namespace FileSink {
std::shared_ptr<OutputSinkPlugin> FileFdSinkPluginCreator(const std::string& name)
{
    return std::make_shared<FileFdSinkPlugin>(name);
}

Status FileFdSinkRegister(const std::shared_ptr<Register>& reg)
{
    OutputSinkPluginDef definition;
    definition.outputType = OutputType::FD;
    definition.name = "file_fd_sink";
    definition.description = "file fd sink";
    definition.rank = 100; // 100
    definition.outputType = OutputType::FD;
    definition.creator = FileFdSinkPluginCreator;
    return reg->AddPlugin(definition);
}

PLUGIN_DEFINITION(FileFdSink, LicenseType::APACHE_V2, FileFdSinkRegister, [] {});

FileFdSinkPlugin::FileFdSinkPlugin(std::string name)
    : OutputSinkPlugin(std::move(name)), fd_(-1), isSeekable_(true)
{
}

Status FileFdSinkPlugin::SetSink(const Plugin::ValueType& sink)
{
    if (!sink.SameTypeWith(typeid(int32_t))) {
        MEDIA_LOG_E("Invalid parameter to file_fd_sink plugin");
        return Status::ERROR_INVALID_PARAMETER;
    }
    fd_ =  Plugin::AnyCast<int32_t>(sink);
    return Status::OK;
}

bool FileFdSinkPlugin::IsSeekable()
{
    return isSeekable_;
}

Status FileFdSinkPlugin::SeekTo(uint64_t offset)
{
    FALSE_RET_V_MSG_E(fd_ != -1, Status::ERROR_WRONG_STATE, "no valid fd.");
    int64_t ret = lseek(fd_, offset, SEEK_SET);
    if (ret != -1) {
        MEDIA_LOG_I("now seek to " PUBLIC_LOG_D64, ret);
    } else {
        MEDIA_LOG_E("seek to " PUBLIC_LOG_U64 " failed due to " PUBLIC_LOG_S, offset, strerror(errno));
    }
    return Status::ERROR_UNKNOWN;
}

Status FileFdSinkPlugin::Write(const std::shared_ptr<Buffer>& buffer)
{
    MEDIA_LOG_D("Write begin");
    if (buffer == nullptr || buffer->IsEmpty()) {
        return Status::OK;
    }
    auto bufferData = buffer->GetMemory();
    write(fd_, bufferData->GetReadOnlyData(), bufferData->GetSize());
    return Status::OK;
}

Status FileFdSinkPlugin::Flush()
{
    MEDIA_LOG_D("OUT");
    return Status::OK;
}
} // namespace FileSink
} // namespace Plugin
} // namespace Media
} // namespace OHOS