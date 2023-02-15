/*
 * Copyright (c) 2023-2023 Huawei Device Co., Ltd.
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
#if !defined(OHOS_LITE) && defined(VIDEO_SUPPORT)
#include "hdi_codec_manager.h"
#include <hdf_base.h>
#include "common/plugin_caps_builder.h"
#include "constants.h"
#include "foundation/log.h"
#include "hdi_codec_adapter.h"

namespace OHOS {
namespace Media {
namespace Plugin {
namespace CodecAdapter {
std::shared_ptr<CodecManager>  g_codecMgr {nullptr};
Status RegisterHdiAdapterPlugins(const std::shared_ptr<OHOS::Media::Plugin::Register>& reg)
{
    g_codecMgr = std::make_shared<HdiCodecManager>();
    return g_codecMgr->RegisterCodecPlugins(reg);
}

void UnRegisterHdiAdapterPlugins()
{
    g_codecMgr->UnRegisterCodecPlugins();
    g_codecMgr = nullptr;
}

PLUGIN_DEFINITION(HdiCodecAdapter, LicenseType::APACHE_V2, RegisterHdiAdapterPlugins, UnRegisterHdiAdapterPlugins);

HdiCodecManager::HdiCodecManager()
{
    Init();
}

HdiCodecManager::~HdiCodecManager()
{
    Reset();
}

int32_t HdiCodecManager::CreateComponent(const Plugin::Any& component, uint32_t& id, std::string name,
                                         const Plugin::Any& appData, const Plugin::Any& callbacks)
{
    if (!mgr_) {
        Init();
        FALSE_RETURN_V_MSG(mgr_ != nullptr, HDF_FAILURE, "mgr is nullptr");
    }
    auto codecComponent = Plugin::AnyCast<CodecComponentType**>(component);
    FALSE_RETURN_V_MSG(codecComponent != nullptr, HDF_FAILURE, "component is nullptr");
    auto ret = mgr_->CreateComponent(codecComponent, &id, const_cast<char *>(name.c_str()),
        Plugin::AnyCast<int64_t>(appData), Plugin::AnyCast<CodecCallbackType*>(callbacks));
    if (ret == HDF_SUCCESS) {
        handleMap_[*codecComponent] = id;
    }
    return ret;
}

int32_t HdiCodecManager::DestroyComponent(const Plugin::Any& component, uint32_t id)
{
    FALSE_RETURN_V_MSG(mgr_ != nullptr, HDF_FAILURE, "mgr_ is nullptr");
    auto codecComponent = Plugin::AnyCast<CodecComponentType*>(component);
    auto iter = handleMap_.find(codecComponent);
    FALSE_RETURN_V_MSG(iter != handleMap_.end(), HDF_SUCCESS, "The handle has been released!");
    FALSE_RETURN_V_MSG(iter->second == id, HDF_FAILURE, "Handle and id do not match!");
    int32_t ret =  mgr_->DestroyComponent(id);
    if (ret == HDF_SUCCESS) {
        handleMap_.erase(iter);
    }
    return ret;
}

Status HdiCodecManager::RegisterCodecPlugins(const std::shared_ptr<OHOS::Media::Plugin::Register>& reg)
{
    std::string packageName = "HdiAdapter";
    if (!mgr_) {
        MEDIA_LOG_E("Codec package " PUBLIC_LOG_S " has no valid component manager", packageName.c_str());
        return Status::ERROR_INVALID_DATA;
    }
    InitCaps();
    for (auto codecCapability : codecCapabilitys_) {
        CodecPluginDef def;
        def.rank = 100; // 100 default rank
        def.codecMode = CodecMode::HARDWARE;
        def.pluginType = codecCapability.pluginType;
        def.name = packageName + "." + codecCapability.name;
        def.inCaps = codecCapability.inCaps;
        def.outCaps = codecCapability.outCaps;
        def.creator = [] (const std::string& name) -> std::shared_ptr<CodecPlugin> {
            return std::make_shared<HdiCodecAdapter>(name, g_codecMgr);
        };
        if (reg->AddPlugin(def) != Status::OK) {
            MEDIA_LOG_E("Add plugin " PUBLIC_LOG_S " failed", def.name.c_str());
        }
    }
    return Status::OK;
}

Status HdiCodecManager::UnRegisterCodecPlugins()
{
    return Status::OK;
}

void HdiCodecManager::Init()
{
    mgr_ = GetCodecComponentManager();
}

void HdiCodecManager::Reset()
{
    CodecComponentManagerRelease();
    mgr_ = nullptr;
    handleMap_.clear();
}

void HdiCodecManager::AddHdiCap(const CodecCompCapability& hdiCap)
{
    CodecCapability codecCapability;
    CapabilityBuilder incapBuilder;
    incapBuilder.SetMime(GetCodecMime(hdiCap.role));
    CapabilityBuilder outcapBuilder;
    outcapBuilder.SetMime(MEDIA_MIME_VIDEO_RAW);
    outcapBuilder.SetVideoPixelFormatList(GetCodecFormats(hdiCap.port.video));
    codecCapability.inCaps.push_back(incapBuilder.Build());
    codecCapability.outCaps.push_back(outcapBuilder.Build());
    codecCapability.pluginType = GetCodecType(hdiCap.type);
    codecCapability.name = hdiCap.compName;
    codecCapabilitys_.push_back(codecCapability);
}

void HdiCodecManager::InitCaps()
{
    auto len = mgr_->GetComponentNum();
    CodecCompCapability hdiCaps[len];
    auto ret = mgr_->GetComponentCapabilityList(hdiCaps, len);
    FALSE_RETURN_MSG(ret == HDF_SUCCESS, "GetComponentCapabilityList fail");
    for (auto i = 0; i < len; ++i) {
        AddHdiCap(hdiCaps[i]);
    }
}

std::vector<VideoPixelFormat> HdiCodecManager::GetCodecFormats(const CodecVideoPortCap& port)
{
    int32_t index = 0;
    std::vector<VideoPixelFormat> formats;
    while (index < PIX_FORMAT_NUM && port.supportPixFmts[index] > 0) {
        switch (port.supportPixFmts[index]) {
            case PIXEL_FMT_YCBCR_420_SP:
                formats.push_back(VideoPixelFormat::NV12);
                break;
            case PIXEL_FMT_YCRCB_420_SP:
                formats.push_back(VideoPixelFormat::NV21);
                break;
            case PIXEL_FMT_YCBCR_420_P:
                formats.push_back(VideoPixelFormat::YUV420P);
                break;
            case PIXEL_FMT_RGBA_8888:
                formats.push_back(VideoPixelFormat::RGBA);
                break;
            default:
                MEDIA_LOG_W("Unknown Format" PUBLIC_LOG_D32, port.supportPixFmts[index]);
        }
        index++;
    }
    return formats;
}

PluginType HdiCodecManager::GetCodecType(const CodecType& hdiType)
{
    switch (hdiType) {
        case VIDEO_DECODER:
            return PluginType::VIDEO_DECODER;
        case VIDEO_ENCODER:
            return PluginType::VIDEO_ENCODER;
        case AUDIO_DECODER:
            return PluginType::AUDIO_DECODER;
        case AUDIO_ENCODER:
            return PluginType::AUDIO_ENCODER;
        default:
            return PluginType::INVALID_TYPE;
    }
}

std::string HdiCodecManager::GetCodecMime(const AvCodecRole& role)
{
    switch (role) {
        case MEDIA_ROLETYPE_VIDEO_AVC:
            return MEDIA_MIME_VIDEO_H264;
        default:
            return "video/unknown";
    }
}
} // namespace CodecAdapter
} // namespace Plugin
} // namespace Media
} // namespace OHOS
#endif