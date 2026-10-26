/*
 * Copyright (c) 2026 HiSilicon (Shanghai) Technologies Co., Ltd.
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

#include "engines/gfx/hi3519/hi3519_engine.h"
#include "gfx_utils/graphic_log.h"
#include "hals/gfx_engines.h"
#include "lite_wm_type.h"

namespace OHOS {
const int16_t HARDWARE_ACC_WIDTH_LIMIT = 50;  // 50: Minimum width for hardware acceleration, in pixels
const int16_t HARDWARE_ACC_HEIGHT_LIMIT = 50; // 50: Minimum height for hardware acceleration, in pixels

__attribute__((constructor)) void RegisterHi3519GfxEngine()
{
    BaseGfxEngine::InitGfxEngine(new Hi3519GfxEngine());
}

ImagePixelFormat Hi3519GfxEngine::GetPixelFormatByColorMode(ColorMode colorMode)
{
    ImagePixelFormat format = IMAGE_PIXEL_FORMAT_NONE;
    switch (colorMode) {
        case ARGB8888:
            format = IMAGE_PIXEL_FORMAT_ARGB8888;
            break;
        case RGB888:
            format = IMAGE_PIXEL_FORMAT_RGB888;
            break;
        case RGB565:
            format = IMAGE_PIXEL_FORMAT_RGB565;
            break;
        case ARGB1555:
            format = IMAGE_PIXEL_FORMAT_ARGB1555;
            break;
        default:
            GRAPHIC_LOGE("current color mode not support!");
            break;
    }
    return format;
}

uint8_t GetByteSizeByColorMode(uint8_t colorMode)
{
    switch (colorMode) {
        case ARGB8888:
            return 4; // 4: 4 Byte
        case RGB888:
            return 3; // 3: 3 Byte
        case RGB565:
        case ARGB1555:
        case ARGB4444:
            return 2; // 2: 2 Byte
        default:
            return 0;
    }
}

void Hi3519GfxEngine::Blit(BufferInfo& dst, const Point& dstPos, const BufferInfo& src,
    const Rect& subRect, const BlendOption& blendOption)
{
#if defined(ENABLE_GFX_ENGINES) && ENABLE_GFX_ENGINES
    LiteSurfaceData srcData;
    srcData.phyAddr = (uint8_t *)src.phyAddr;
    srcData.width = src.width;
    srcData.height = src.height;
    srcData.stride = src.stride;
    srcData.pixelFormat = GetPixelFormatByColorMode(src.mode);

    LiteSurfaceData dstData;
    dstData.phyAddr = (uint8_t *)dst.phyAddr;
    dstData.width = dst.width;
    dstData.height = dst.height;
    dstData.stride = dst.stride;
    dstData.pixelFormat = GetPixelFormatByColorMode(dst.mode);
    Rect dstRect;
    dstRect.SetX(dstPos.x);
    dstRect.SetY(dstPos.y);
    dstRect.SetWidth(subRect.GetWidth());
    dstRect.SetHeight(subRect.GetHeight());
    if (GfxEngines::GetInstance()->GfxBlitWithOpt(srcData, src.rect, dstData, dstRect, blendOption.opacity)) {
        return;
    }
#endif
    SoftEngine::Blit(dst, dstPos, src, subRect, blendOption);
}

void Hi3519GfxEngine::Fill(BufferInfo& dst,
                           const Rect& fillArea,
                           const ColorType color,
                           const OpacityType opacity)
{
#if defined(ENABLE_GFX_ENGINES) && ENABLE_GFX_ENGINES
    if ((opacity != OPA_OPAQUE) && (fillArea.GetWidth() >= HARDWARE_ACC_WIDTH_LIMIT) &&
        (fillArea.GetHeight() >= HARDWARE_ACC_HEIGHT_LIMIT)) {
        LiteSurfaceData data;
        data.phyAddr = static_cast<uint8_t *>(dst.phyAddr);
        data.width = dst.width;
        data.height = dst.height;
        data.stride = dst.stride;
        data.pixelFormat = IMAGE_PIXEL_FORMAT_ARGB8888;
        if (GfxEngines::GetInstance()->GfxFillArea(data, fillArea, color, opacity)) {
            return;
        }
    }
#endif
    SoftEngine::Fill(dst, fillArea, color, opacity);
}

#if defined(ENABLE_GFX_ENGINES) && ENABLE_GFX_ENGINES
// Add 0.5 before the int16_t truncation to round scale-derived coordinates explicitly.
constexpr double ROUND_HALF = 0.5;

// Run the transform blit through the TDE hardware engine. Returns false when the
// request cannot be served by hardware, in which case the caller falls back to the
// software engine.
static bool BlitTransformByGfx(BufferInfo& dst, const Rect& mask, const Point& position, OpacityType opacity,
    const TransformMap& transMap, const TransformDataInfo& dataInfo,
    ImagePixelFormat srcPixelFormat, ImagePixelFormat dstPixelFormat)
{
    LiteSurfaceData srcData;
    srcData.phyAddr = const_cast<uint8_t*>(dataInfo.phyAddr);
    srcData.width = dataInfo.header.width;
    srcData.height = dataInfo.header.height;
    uint32_t rowSize = srcData.width * GetByteSizeByColorMode(dataInfo.header.colorMode);
    // The decoded data is tightly packed row by row. TDE requires an aligned stride, so fall back to
    // the software engine when the row size is not 4-byte aligned.
    if ((rowSize % 4) != 0) { // 4: TDE stride alignment in bytes
        return false;
    }
    srcData.stride = rowSize;
    srcData.pixelFormat = srcPixelFormat;

    LiteSurfaceData dstData;
    dstData.phyAddr = (uint8_t *)dst.phyAddr;
    dstData.width = dst.width;
    dstData.height = dst.height;
    dstData.stride = dst.stride;
    dstData.pixelFormat = dstPixelFormat;

    Rect trans = transMap.GetBoxRect();
    if ((trans.GetWidth() == 0) || (trans.GetHeight() == 0)) {
        GRAPHIC_LOGE("transMap rect error, size %u %u\n", trans.GetWidth(), trans.GetHeight());
        return false;
    }
    double xScale = 1.0f * trans.GetWidth() / dataInfo.header.width;
    double yScale = 1.0f * trans.GetHeight() / dataInfo.header.height;
    trans.SetX(trans.GetX() + position.x);
    trans.SetY(trans.GetY() + position.y);
    Rect dstRect;
    if (!dstRect.Intersect(trans, mask)) {
        return false;
    }

    Rect srcRect;
    int32_t leftOffset = dstRect.GetLeft() - trans.GetLeft();
    int32_t topOffset = dstRect.GetTop() - trans.GetTop();
    srcRect.SetLeft(static_cast<int16_t>(leftOffset / xScale + ROUND_HALF));
    srcRect.SetTop(static_cast<int16_t>(topOffset / yScale + ROUND_HALF));
    srcRect.SetWidth(static_cast<int16_t>(dstRect.GetWidth() / xScale + ROUND_HALF));
    srcRect.SetHeight(static_cast<int16_t>(dstRect.GetHeight() / yScale + ROUND_HALF));
    // Clip the source rect against the source image bounds to avoid out-of-bound physical memory reads
    Rect srcBound(0, 0, dataInfo.header.width - 1, dataInfo.header.height - 1);
    Rect clipped;
    if (!clipped.Intersect(srcRect, srcBound)) {
        return false;
    }
    return GfxEngines::GetInstance()->GfxBlitWithOpt(srcData, clipped, dstData, dstRect, opacity);
}
#endif

void Hi3519GfxEngine::DrawTransform(BufferInfo& dst, const Rect& mask, const Point& position, ColorType color,
    OpacityType opacity, const TransformMap& transMap, const TransformDataInfo& dataInfo)
{
#if defined(ENABLE_GFX_ENGINES) && ENABLE_GFX_ENGINES
    if ((transMap.GetRotateAngle() == 0) && (dataInfo.phyAddr != nullptr) &&
        (dataInfo.header.width != 0) && (dataInfo.header.height != 0)) {
        if (BlitTransformByGfx(dst, mask, position, opacity, transMap, dataInfo,
            GetPixelFormatByColorMode(static_cast<ColorMode>(dataInfo.header.colorMode)),
            GetPixelFormatByColorMode(dst.mode))) {
            return;
        }
    }
#endif
    SoftEngine::DrawTransform(dst, mask, position, color, opacity, transMap, dataInfo);
}
} // namespace OHOS