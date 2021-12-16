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
#ifndef HISTREAMER_PIPELINE_FILE_SINK_FILTER_H
#define HISTREAMER_PIPELINE_FILE_SINK_FILTER_H

#include "filter_base.h"

namespace OHOS {
namespace Media {
namespace Pipeline {
class FileSinkFilter : public FilterBase {
public:
    explicit FileSinkFilter(std::string name);
    ~FileSinkFilter() override;
    ErrorCode SetOutputPath(const std::string& path);
    ErrorCode SetFd(int32_t fd);
    ErrorCode SetMaxFileSize(uint64_t maxFileSize);
};
} // Pipeline
} // Media
} // OHOS
#endif // HISTREAMER_PIPELINE_FILE_SINK_FILTER_H
#endif
