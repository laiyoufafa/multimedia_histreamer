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

#ifndef HISTREAMER_HIPLAYER_PREPARE_STATE_H
#define HISTREAMER_HIPLAYER_PREPARE_STATE_H

#include <memory>
#include "foundation/log.h"
#include "pipeline/core/error_code.h"
#include "play_executor.h"
#include "state.h"

namespace OHOS {
namespace Media {
class PreparingState : public State {
public:
    explicit PreparingState(StateId stateId, PlayExecutor& executor) : State(stateId, "PreparingState", executor)
    {
    }

    ~PreparingState() override = default;

    std::tuple<ErrorCode, Action> Prepare() override
    {
        Action nextAction = Action::ACTION_BUTT;
        auto rtv = executor_.PrepareFilters();
        if (rtv != ErrorCode::SUCCESS) {
            nextAction = Action::TRANS_TO_INIT;
        }
        return {rtv, nextAction};
    }

    std::tuple<ErrorCode, Action> Play() override
    {
        MEDIA_LOG_W("Play received in preparing state.");
        return {ErrorCode::SUCCESS, Action::ACTION_PENDING};
    }

    std::tuple<ErrorCode, Action> Seek(const Plugin::Any& param) override
    {
        MEDIA_LOG_D("Seek in preparing state.");
        std::tuple<ErrorCode, Action> err {ErrorCode::ERROR_INVALID_PARAMETER_TYPE, Action::ACTION_BUTT};
        FALSE_RETURN_V(param.SameTypeWith(typeid(SeekInfo)), err);
        auto info = Plugin::AnyCast<SeekInfo>(param);
        auto ret = executor_.DoSeek(true, info.hstTime, info.mode);
        return {ret, Action::ACTION_BUTT};
    }

    std::tuple<ErrorCode, Action> Stop() override
    {
        return {ErrorCode::SUCCESS, Action::TRANS_TO_INIT};
    }

    std::tuple<ErrorCode, Action> OnReady() override
    {
        return {ErrorCode::SUCCESS, Action::TRANS_TO_READY};
    }
};
} // namespace Media
} // namespace OHOS
#endif
