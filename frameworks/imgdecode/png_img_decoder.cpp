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

#include "imgdecode/png_img_decoder.h"

#if defined(ENABLE_PNG) && ENABLE_PNG

#include <cstdio>
#include <setjmp.h>

#include "draw/draw_utils.h" // DrawUtils::GetPxSizeByColorMode
#include "gfx_utils/graphic_log.h"
#include "gfx_utils/mem_api.h"

namespace OHOS {

template<typename T>
void FreeResource(T*& ptr)
{
    UIFree(ptr);
    ptr = nullptr;
}

// 为了兼容数组指针类型
template<typename T>
void FreeResource(T**& ptr)
{
    UIFree(ptr);
    ptr = nullptr;
}

RetCode PngImgDecoder::Open(ImgResDsc& dsc)
{
    PngSession* session = static_cast<PngSession*>(UIMalloc(sizeof(PngSession)));
    if (session == nullptr) {
        GRAPHIC_LOGE("Alloc png session failed\n");
        return RetCode::FAIL;
    }
    session->fp = nullptr;
    session->png = nullptr;
    session->info = nullptr;

    session->fp = fopen(dsc.path, "rb");
    if (session->fp == nullptr) {
        GRAPHIC_LOGE("open file:%s failed\n", dsc.path);
        UIFree(session);
        return RetCode::FAIL;
    }

    dsc.session = session;
    dsc.imgInfo.data = nullptr;
    dsc.inCache = false;
    dsc.fd = -1; // don't use fd here

    return RetCode::OK;
}

RetCode PngImgDecoder::Close(ImgResDsc& dsc)
{
    DestroyDecoder(dsc);

    if (dsc.imgInfo.data != nullptr) {
        ImageCacheFree(dsc.imgInfo);
        dsc.imgInfo.data = nullptr;
    }

    UIFree(dsc.session);
    dsc.session = nullptr;
    dsc.inCache = false;

    return RetCode::OK;
}

RetCode PngImgDecoder::CreateDecoder(ImgResDsc& dsc)
{
    PngSession* session = static_cast<PngSession*>(dsc.session);
    if ((session == nullptr) || (session->fp == nullptr)) {
        GRAPHIC_LOGE("File is not opened, call Open first\n");
        return RetCode::FAIL;
    }

    session->png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (session->png == nullptr) {
        return RetCode::FAIL;
    }

    // Errors raised by the libpng calls below must land in a frame that is
    // still alive, so set up error handling right after the read struct exists.
    if (setjmp(png_jmpbuf(session->png)) != 0) {
        png_destroy_read_struct(&(session->png),
            (session->info != nullptr) ? &(session->info) : nullptr, nullptr);
        session->png = nullptr;
        session->info = nullptr;
        GRAPHIC_LOGE("Read png header failed\n");
        return RetCode::FAIL;
    }

    session->info = png_create_info_struct(session->png);
    if (session->info == nullptr) {
        png_destroy_read_struct(&(session->png), nullptr, nullptr);
        session->png = nullptr;
        return RetCode::FAIL;
    }

    png_init_io(session->png, session->fp);
    png_read_info(session->png, session->info);

    return RetCode::OK;
}

void PngImgDecoder::DestroyDecoder(ImgResDsc& dsc)
{
    PngSession* session = static_cast<PngSession*>(dsc.session);
    if (session == nullptr) {
        return;
    }

    if (session->png != nullptr) {
        if (session->info != nullptr) {
            png_destroy_read_struct(&(session->png), &(session->info), nullptr);
        } else {
            png_destroy_read_struct(&(session->png), nullptr, nullptr);
        }
        session->png = nullptr;
        session->info = nullptr;
    }

    if (session->fp != nullptr) {
        fclose(session->fp);
        session->fp = nullptr;
    }
}

RetCode PngImgDecoder::GetHeader(ImgResDsc& dsc)
{
    if (CreateDecoder(dsc) != RetCode::OK) {
        GRAPHIC_LOGE("Decoder is created failed\n");
        return RetCode::FAIL;
    }

    PngSession* session = static_cast<PngSession*>(dsc.session);
    dsc.imgInfo.header.width = png_get_image_width(session->png, session->info);
    dsc.imgInfo.header.height = png_get_image_height(session->png, session->info);
    dsc.imgInfo.header.colorMode = ARGB8888;

    return RetCode::OK;
}

RetCode PngImgDecoder::UpdateInfo(ImgResDsc& dsc)
{
    PngSession* session = static_cast<PngSession*>(dsc.session);
    uint8_t colorType = png_get_color_type(session->png, session->info);
    uint8_t bitDepth = png_get_bit_depth(session->png, session->info);
    if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8) { // 8: Expand grayscale images to the full 8 bits
        png_set_expand_gray_1_2_4_to_8(session->png);
    }
    if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(session->png);
    }
    if (colorType == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(session->png);
    }
    if (bitDepth == 16) { // 16: Chop 16-bit depth images to 8-bit depth
        png_set_strip_16(session->png);
    }

    // A tRNS chunk already adds an alpha channel, so the filler must not be
    // appended again for such images, otherwise the output gets 5 channels.
    bool hasAlpha = ((colorType & PNG_COLOR_MASK_ALPHA) != 0) ||
        (png_get_valid(session->png, session->info, PNG_INFO_tRNS) != 0);
    if (png_get_valid(session->png, session->info, PNG_INFO_tRNS)) {
        png_set_tRNS_to_alpha(session->png);
    }
    if (!hasAlpha) {
        png_set_add_alpha(session->png, 0xFF, PNG_FILLER_AFTER);
    }
    png_set_interlace_handling(session->png);
    png_read_update_info(session->png, session->info);

    // The BGRA conversion below assumes 8-bit RGBA output; anything else
    // would read out of the row bounds.
    if ((png_get_channels(session->png, session->info) != 4) || // 4: RGBA channels
        (png_get_bit_depth(session->png, session->info) != 8)) { // 8: bits per channel
        GRAPHIC_LOGE("Unsupported png pixel format\n");
        return RetCode::FAIL;
    }

    return RetCode::OK;
}

// Allocate one buffer per decoded row; release the row buffers on failure.
static bool AllocPngRowPointers(png_bytep* rowPointer, uint16_t height, png_uint_32 rowBytes)
{
    for (uint16_t y = 0; y < height; y++) {
        rowPointer[y] = static_cast<png_byte*>(UIMalloc(rowBytes));
        if (rowPointer[y] == nullptr) {
            for (uint16_t i = 0; i < y; i++) {
                FreeResource(rowPointer[i]);
            }
            return false;
        }
    }
    return true;
}

// Release all row buffers and the pointer array itself.
static void FreePngRowPointers(png_bytep* rowPointer, uint16_t height)
{
    for (uint16_t y = 0; y < height; y++) {
        FreeResource(rowPointer[y]);
    }
    FreeResource(rowPointer);
}

// Convert decoded RGBA rows into the BGRA layout of the image cache and
// release the row buffers afterwards.
static void ConvertPngRowsToBgra(uint8_t* srcData, png_bytep* rowPointer, uint16_t width, uint16_t height,
    uint8_t pixelByteSize)
{
    uint32_t n = 0;
    for (uint16_t y = 0; y < height; y++) {
        png_bytep row = rowPointer[y];
        for (uint16_t x = 0; x < width * pixelByteSize; x += pixelByteSize) {
            srcData[n++] = row[x + 2]; // 2: B channel
            srcData[n++] = row[x + 1]; // 1: G channel
            srcData[n++] = row[x + 0]; // 0: R channel
            srcData[n++] = row[x + 3]; // 3: Alpha channel
        }
    }
    FreePngRowPointers(rowPointer, height);
}

RetCode PngImgDecoder::ReadToCache(ImgResDsc& dsc)
{
    PngSession* session = static_cast<PngSession*>(dsc.session);
    if ((session == nullptr) || (session->png == nullptr)) {
        GRAPHIC_LOGE("Decoder is not created, call GetHeader first\n");
        return RetCode::FAIL;
    }

    // Re-arm error handling in this still-alive frame: the one installed by
    // CreateDecoder returned long ago. Left-over resources are released by Close.
    if (setjmp(png_jmpbuf(session->png)) != 0) {
        GRAPHIC_LOGE("Decode png image failed\n");
        return RetCode::FAIL;
    }

    uint8_t pixelByteSize = DrawUtils::GetPxSizeByColorMode(ARGB8888) >> 3; // 3: Shift right 3 bits
    uint16_t width = dsc.imgInfo.header.width;
    uint16_t height = dsc.imgInfo.header.height;
    uint32_t dataSize = height * width * pixelByteSize;

    if (UpdateInfo(dsc) != RetCode::OK) {
        DestroyDecoder(dsc);
        return RetCode::FAIL;
    }
    png_uint_32 rowBytes = png_get_rowbytes(session->png, session->info);
    png_bytep* rowPointer = static_cast<png_bytep*>(UIMalloc(sizeof(png_bytep) * height));
    if ((rowPointer == nullptr) || !AllocPngRowPointers(rowPointer, height, rowBytes)) {
        FreeResource(rowPointer);
        DestroyDecoder(dsc);
        return RetCode::FAIL;
    }
    png_read_image(session->png, rowPointer);

    dsc.imgInfo.dataSize = dataSize;
    uint8_t* srcData = static_cast<uint8_t*>(ImageCacheMalloc(dsc.imgInfo));
    dsc.imgInfo.data = srcData;
    if (srcData == nullptr) {
        FreePngRowPointers(rowPointer, height);
        DestroyDecoder(dsc);
        return RetCode::FAIL;
    }
    ConvertPngRowsToBgra(srcData, rowPointer, width, height, pixelByteSize);

    DestroyDecoder(dsc);
    dsc.inCache = true;
    return RetCode::OK;
}
} // namespace OHOS

#endif // ENABLE_PNG
