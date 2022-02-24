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
#define HST_LOG_TAG "DataPacker"
#define MEDIA_LOG_DEBUG 0

#include "data_packer.h"
#include <cstring>
#include "foundation/log.h"
#include "filters/common/dump_buffer.h"

namespace OHOS {
namespace Media {
#define EXEC_WHEN_GET(isGet, exec)     \
    do {                               \
        if (isGet) {                   \
            exec;                      \
        }                              \
    } while (0)

static constexpr DataPacker::Position INVALID_POSITION = {-1, 0, 0};

DataPacker::DataPacker() : mutex_(), que_(), size_(0), mediaOffset_(0), pts_(0), dts_(0),
    prevGet_{INVALID_POSITION, INVALID_POSITION},
    currentGet_ {INVALID_POSITION, INVALID_POSITION},
    capacity_(30) // capacity 30
{
    MEDIA_LOG_I("DataPacker ctor...");
}

DataPacker::~DataPacker()
{
    MEDIA_LOG_I("DataPacker dtor...");
}

inline static size_t AudioBufferSize(AVBufferPtr& ptr)
{
    return ptr->GetMemory()->GetSize();
}

inline static uint8_t* AudioBufferWritableData(AVBufferPtr& ptr, size_t size, size_t position = 0)
{
    return ptr->GetMemory()->GetWritableAddr(size, position);
}

inline static const uint8_t* AudioBufferReadOnlyData(AVBufferPtr& ptr)
{
    return ptr->GetMemory()->GetReadOnlyData();
}

void DataPacker::PushData(AVBufferPtr bufferPtr, uint64_t offset)
{
    size_t bufferSize = AudioBufferSize(bufferPtr);
    MEDIA_LOG_D("DataPacker PushData begin... buffer (offset " PUBLIC_LOG_U64 ", size " PUBLIC_LOG_ZU ")",
                offset, bufferSize);
    DUMP_BUFFER2LOG("DataPacker Push", bufferPtr, offset);
    FALSE_RET_MSG(bufferSize > 0, "Can not push zero length buffer.");

    OSAL::ScopedLock lock(mutex_);
    if (que_.size() >= capacity_) {
        MEDIA_LOG_D("DataPacker is full, waiting for pop.");
        cvFull_.Wait(lock, [this] { return que_.size() < capacity_; });
    }

    size_ += AudioBufferSize(bufferPtr);
    if (que_.empty()) {
        mediaOffset_ = offset;
        dts_ = bufferPtr->dts;
        pts_ = bufferPtr->pts;
    }
    que_.emplace_back(std::move(bufferPtr));
    cvEmpty_.NotifyOne();
    MEDIA_LOG_D("DataPacker PushData end... " PUBLIC_LOG_S, ToString().c_str());
}

// curOffset: the offset end of this dataPacker. if IsDataAvailable() false, we can get data from source from curOffset
bool DataPacker::IsDataAvailable(uint64_t offset, uint32_t size, uint64_t &curOffset)
{
    MEDIA_LOG_D("dataPacker (offset " PUBLIC_LOG_U64 ", size " PUBLIC_LOG_U32 "), curOffsetEnd is " PUBLIC_LOG_U64,
                mediaOffset_, size_.load(), mediaOffset_ + size_.load());
    MEDIA_LOG_D(PUBLIC_LOG_S, ToString().c_str());
    OSAL::ScopedLock lock(mutex_);
    auto curOffsetTemp = mediaOffset_;
    if (que_.empty() || offset < curOffsetTemp || offset > curOffsetTemp + size_) { // 原有数据无法命中, 则删除原有数据
        curOffset = offset;
        FlushInternal();
        MEDIA_LOG_D("IsDataAvailable false, offset not in cached data, clear it.");
        return false;
    }
    size_t bufCnt = que_.size();
    uint64_t offsetEnd = offset + size;
    uint64_t curOffsetEnd = mediaOffset_ + AudioBufferSize(que_.front());
    if (bufCnt == 1) {
        curOffset = curOffsetEnd;
        MEDIA_LOG_D("IsDataAvailable bufCnt == 1, result " PUBLIC_LOG_D32, offsetEnd <= curOffsetEnd);
        return offsetEnd <= curOffsetEnd;
    }
    auto preOffsetEnd = curOffsetEnd;
    for (size_t i = 1; i < bufCnt; ++i) {
        curOffsetEnd = preOffsetEnd + AudioBufferSize(que_[i]);
        if (curOffsetEnd >= offsetEnd) {
            MEDIA_LOG_D("IsDataAvailable true, last buffer index " PUBLIC_LOG_ZU ", offsetEnd " PUBLIC_LOG_U64
                ", curOffsetEnd " PUBLIC_LOG_U64, i, offsetEnd, curOffsetEnd);
            return true;
        } else {
            preOffsetEnd = curOffsetEnd;
        }
    }
    if (preOffsetEnd >= offsetEnd) {
        MEDIA_LOG_D("IsDataAvailable true, use all buffers, last buffer index " PUBLIC_LOG_ZU ", offsetEnd "
            PUBLIC_LOG_U64 ", curOffsetEnd " PUBLIC_LOG_U64, bufCnt - 1, offsetEnd, curOffsetEnd);
        return true;
    }
    curOffset = preOffsetEnd;
    MEDIA_LOG_D("IsDataAvailable false, offsetEnd " PUBLIC_LOG_U64 ", curOffsetEnd " PUBLIC_LOG_U64,
                offsetEnd, preOffsetEnd);
    return false;
}

bool DataPacker::PeekRange(uint64_t offset, uint32_t size, AVBufferPtr& bufferPtr)
{
    OSAL::ScopedLock lock(mutex_);
    if (que_.empty()) {
        MEDIA_LOG_D("DataPacker is empty, waiting for push.");
        cvEmpty_.Wait(lock, [this] { return !que_.empty(); });
    }

    return PeekRangeInternal(offset, size, bufferPtr, false);
}

// Should call IsDataAvailable() before to make sure there is enough buffer to copy.
// offset : the offset (of the media file) to peek ( 要peek的数据起始位置 在media file文件 中的 offset )
// size : the size of data to peek
// bufferPtr : out buffer
// isGet : is it called from GetRange.
bool DataPacker::PeekRangeInternal(uint64_t offset, uint32_t size, AVBufferPtr &bufferPtr, bool isGet)
{
    MEDIA_LOG_D("PeekRangeInternal (offset, size) = (" PUBLIC_LOG_U64 ", " PUBLIC_LOG_U32 ")...", offset, size);
    int32_t usedCount = 1, startIndex = 0; // The index of buffer that we first use
    size_t copySize = 0;
    uint32_t needCopySize = size, firstBufferOffset = 0, lastBufferOffsetEnd = 0;
    uint8_t* dstPtr = AudioBufferWritableData(bufferPtr, needCopySize);
    FALSE_RETURN_V(dstPtr != nullptr, false);

    auto offsetEnd = offset + needCopySize;
    auto curOffsetEnd = mediaOffset_ + AudioBufferSize(que_[startIndex]);
    if (offsetEnd <= curOffsetEnd) { // first buffer is enough
        auto bufferOffset = static_cast<int32_t>(offset - mediaOffset_);
        FALSE_RET_V_MSG_E(bufferOffset >= 0, false, "Copy buffer start position error.");
        firstBufferOffset = bufferOffset;
        copySize = CopyFirstBuffer(size, startIndex, dstPtr, bufferPtr, bufferOffset);
        needCopySize -= copySize;
        FALSE_LOG_MSG_E(needCopySize == 0, "First buffer is enough, but copySize is not enough");
        lastBufferOffsetEnd = firstBufferOffset + size;
        EXEC_WHEN_GET(isGet, currentGet_ = std::make_pair(Position{startIndex, firstBufferOffset, offset},
            Position{startIndex, lastBufferOffsetEnd, offset + size}));
        return true;
    } else { // first buffer not enough
        // Find the first buffer that should copy
        uint64_t prevOffset; // The media offset of the startIndex buffer start byte
        FALSE_RETURN_V(FindFirstBufferToCopy(offset, startIndex, prevOffset), false);
        auto bufferOffset = static_cast<int32_t>(offset - prevOffset);
        FALSE_RET_V_MSG_E(bufferOffset >= 0, false, "Copy buffer start position error.");
        firstBufferOffset = bufferOffset;
        copySize = CopyFirstBuffer(size, startIndex, dstPtr, bufferPtr, bufferOffset);

        needCopySize -= copySize;
        if (needCopySize == 0) { // First buffer is enough
            lastBufferOffsetEnd = firstBufferOffset + copySize;
            EXEC_WHEN_GET(isGet, currentGet_ = std::make_pair(Position{startIndex, firstBufferOffset, offset},
                Position{startIndex, lastBufferOffsetEnd, offset + size}));
            return true;
        }
        dstPtr += copySize;

        // First buffer is not enough, copy from successive buffers
        usedCount += CopyFromSuccessiveBuffer(prevOffset, offsetEnd, startIndex, dstPtr, needCopySize,
                                              lastBufferOffsetEnd);
    }
    EXEC_WHEN_GET(isGet, currentGet_ = std::make_pair(Position{startIndex, firstBufferOffset, offset},
        Position{startIndex + usedCount - 1, lastBufferOffsetEnd, offset + size}));

    // Update to the real size, especially at the end.
    bufferPtr->GetMemory()->UpdateDataSize(size - needCopySize);
    return true;
}

// Call IsDataAvailable() first before call GetRange
bool DataPacker::GetRange(uint64_t offset, uint32_t size, AVBufferPtr& bufferPtr)
{
    MEDIA_LOG_D("DataPacker GetRange(offset, size) = (" PUBLIC_LOG_U64 ", "
                PUBLIC_LOG_U32 ")...", offset, size);
    DUMP_BUFFER2LOG("GetRange Input", bufferPtr, 0);
    FALSE_RET_V_MSG_E(bufferPtr && (!bufferPtr->IsEmpty()) && bufferPtr->GetMemory()->GetCapacity() >= size, false,
                      "GetRange input bufferPtr empty or capacity not enough.");

    OSAL::ScopedLock lock(mutex_);
    if (que_.empty()) {
        MEDIA_LOG_D("DataPacker is empty, waiting for push");
        cvEmpty_.Wait(lock, [this] { return !que_.empty(); });
    }

    FALSE_RETURN_V(!que_.empty(), false);
    prevGet_ = currentGet_; // store last get position to prevGet_

    FALSE_RETURN_V(PeekRangeInternal(offset, size, bufferPtr, true), false);
    if (meetEos_) {
        FlushInternal();
    } else {
        RemoveOldData();
    }

    if (que_.size() < capacity_) {
        cvFull_.NotifyOne();
    }
    return true;
}

void DataPacker::Flush()
{
    MEDIA_LOG_I("DataPacker Flush called.");
    OSAL::ScopedLock lock(mutex_);
    FlushInternal();
}

void DataPacker::FlushInternal()
{
    MEDIA_LOG_D("DataPacker FlushInternal called.");
    que_.clear();
    size_ = 0;
    mediaOffset_ = 0;
    dts_ = 0;
    pts_ = 0;
    meetEos_ = false;
    prevGet_ = {INVALID_POSITION, INVALID_POSITION};
    currentGet_ = {INVALID_POSITION, INVALID_POSITION};
}

// Remove first removeSize data in the buffer
void DataPacker::RemoveBufferContent(std::shared_ptr<AVBuffer> &buffer, size_t removeSize)
{
    if (removeSize == 0) {
        return;
    }
    auto memory = buffer->GetMemory();
    FALSE_RETURN(removeSize < memory->GetSize());
    auto copySize = memory->GetSize() - removeSize;
    FALSE_LOG_MSG_E(memmove_s(memory->GetWritableAddr(copySize), memory->GetCapacity(),
        memory->GetReadOnlyData(removeSize), copySize) == EOK, "memmove failed.");
    FALSE_RETURN(UpdateWhenFrontDataRemoved(removeSize));
}

// Remove consumed data, and make the remaining data continuous
// Consumed data - between prevGet_.first and currentGet_.first
// In order to make remaining data continuous, also remove the data before prevGet_.first
void DataPacker::RemoveOldData()
{
    // If prevGet_.first >= currentGet_.first, return
    FALSE_RETURN_W(prevGet_.first < currentGet_.first);
    MEDIA_LOG_D("Before RemoveOldData " PUBLIC_LOG_S, ToString().c_str());
    FALSE_LOG(RemoveTo(currentGet_.first));
    if (que_.empty()) {
        mediaOffset_ = 0;
        size_ = 0;
        pts_ = 0;
        dts_ = 0;
    } else {
        pts_ = que_.front()->pts;
        dts_ = que_.front()->dts;
    }
    MEDIA_LOG_D("After RemoveOldData " PUBLIC_LOG_S, ToString().c_str());
}

bool DataPacker::RemoveTo(const Position& position)
{
    MEDIA_LOG_D("Remove to " PUBLIC_LOG_S, position.ToString().c_str());
    size_t removeSize;
    int32_t i = 0;
    while (i < position.index && !que_.empty()) { // Remove all whole buffer before position.index
        removeSize = AudioBufferSize(que_.front());
        FALSE_RETURN_V(UpdateWhenFrontDataRemoved(removeSize), false);
        que_.pop_front();
        i++;
    }
    FALSE_RETURN_V(!que_.empty(), false);

    // The last buffer
    removeSize = AudioBufferSize(que_.front());
    // 1. If whole buffer should be removed
    if (position.bufferOffset >= removeSize) {
        FALSE_RETURN_V(UpdateWhenFrontDataRemoved(removeSize), false);
        que_.pop_front();
        return true;
    }
    // 2. Remove the front part of the buffer data
    RemoveBufferContent(que_.front(), position.bufferOffset);
    return true;
}

bool DataPacker::UpdateWhenFrontDataRemoved(size_t removeSize)
{
    mediaOffset_ += removeSize;
    FALSE_RET_V_MSG_E(size_.load() >= removeSize, false, "Total size(size_ " PUBLIC_LOG_U32
            ") smaller than removeSize(" PUBLIC_LOG_ZU ")", size_.load(), removeSize);
    size_ -= removeSize;
    return true;
}

// offset : from GetRange(offset, size)
// startIndex : out, find the first buffer should copy
// prevOffset : the first copied buffer's media offset.
bool DataPacker::FindFirstBufferToCopy(uint64_t offset, int32_t &startIndex, uint64_t &prevOffset)
{
    startIndex = 0;
    prevOffset= mediaOffset_;
    do {
        if (offset >= prevOffset && offset - prevOffset < AudioBufferSize(que_[startIndex])) {
            return true;
        }
        prevOffset += AudioBufferSize(que_[startIndex]);
        startIndex++;
    } while (static_cast<size_t>(startIndex) < que_.size());
    return false;
}

// size : the GetRange size
// dstPtr : copy data to here
// dstBufferPtr : the AVBuffer contains dstPtr, pass this parameter to update pts / dts.
// bufferOffset : the buffer offset that we start copy
size_t DataPacker::CopyFirstBuffer(size_t size, int32_t index, uint8_t *dstPtr, AVBufferPtr &dstBufferPtr,
                                   int32_t bufferOffset)
{
    auto remainSize = static_cast<int32_t>(AudioBufferSize(que_[index]) - bufferOffset);
    FALSE_RET_V_MSG_E(remainSize > 0, 0, "Copy size can not be negative.");
    size_t copySize = std::min(static_cast<size_t>(remainSize), size);
    NZERO_LOG(memcpy_s(dstPtr, copySize,
        AudioBufferReadOnlyData(que_[index]) + bufferOffset, copySize));

    dstBufferPtr->pts = que_[index]->pts;
    dstBufferPtr->dts = que_[index]->dts;
    return copySize;
}

// prevOffset : the media offset of the first byte in the startIndex + 1 buffer
// offsetEnd : calculate from GetRange(offset, size), offsetEnd = offset + size.
// startIndex : the index start copy data for this GetRange. CopyFromSuccessiveBuffer process from startIndex + 1.
// dstPtr : copy data to here
// needCopySize : in and out, indicate how many bytes still need to copy.
// lastBufferOffsetEnd : the last buffer's buffer offset, that before it are copied to dstPtr.
int32_t DataPacker::CopyFromSuccessiveBuffer(uint64_t prevOffset, uint64_t offsetEnd, int32_t startIndex,
                                             uint8_t *dstPtr, uint32_t &needCopySize, uint32_t &lastBufferOffsetEnd)
{
    size_t copySize;
    int32_t usedCount = 0;
    uint64_t curOffsetEnd;
    prevOffset = prevOffset + AudioBufferSize(que_[startIndex]);
    for (size_t i = startIndex + 1; i < que_.size(); ++i) {
        usedCount++;
        curOffsetEnd = prevOffset + AudioBufferSize(que_[i]);
        if (curOffsetEnd >= offsetEnd) { // This buffer is enough
            NZERO_LOG(memcpy_s(dstPtr, needCopySize, AudioBufferReadOnlyData(que_[i]), needCopySize));
            lastBufferOffsetEnd = needCopySize;
            needCopySize = 0;
            return usedCount; // Finished copy buffer
        } else {
            copySize = AudioBufferSize(que_[i]);
            NZERO_LOG(memcpy_s(dstPtr, copySize, AudioBufferReadOnlyData(que_[i]), copySize));
            dstPtr += copySize;
            needCopySize -= copySize;
            prevOffset += copySize;
        }
    }
    MEDIA_LOG_W("Processed all cached buffers, still not meet offsetEnd, maybe EOS reached.");
    meetEos_ = true;
    return usedCount;
}

std::string DataPacker::ToString() const
{
    return "DataPacker (offset " + std::to_string(mediaOffset_) + ", size " + std::to_string(size_) +
           ", buffer count " + std::to_string(que_.size()) + ")";
}
} // namespace Media
} // namespace OHOS
