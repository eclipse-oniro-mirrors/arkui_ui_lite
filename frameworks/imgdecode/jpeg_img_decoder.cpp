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

#include "imgdecode/jpeg_img_decoder.h"

#if defined(ENABLE_JPEG) && ENABLE_JPEG

#include "draw/draw_utils.h" // DrawUtils::GetPxSizeByColorMode
#include "gfx_utils/graphic_log.h"
#include "gfx_utils/mem_api.h"

namespace OHOS {
void JpegImgDecoder::JpegErrorExit(j_common_ptr cinfo)
{
    JpegErrorMgr* err = reinterpret_cast<JpegErrorMgr*>(cinfo->err);
    // Emit the diagnostic like the default handler does, then jump back into
    // the decoder instead of terminating the process.
    (*cinfo->err->output_message)(cinfo);
    longjmp(err->setjmpBuffer, 1);
}

RetCode JpegImgDecoder::Open(ImgResDsc& dsc)
{
    JpegSession* session = static_cast<JpegSession*>(UIMalloc(sizeof(JpegSession)));
    if (session == nullptr) {
        GRAPHIC_LOGE("Alloc jpeg session failed\n");
        return RetCode::FAIL;
    }
    session->fp = nullptr;
    session->cinfo.err = nullptr;
    session->inited = false;

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

RetCode JpegImgDecoder::Close(ImgResDsc& dsc)
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

RetCode JpegImgDecoder::CreateDecoder(ImgResDsc& dsc)
{
    JpegSession* session = static_cast<JpegSession*>(dsc.session);
    if ((session == nullptr) || (session->fp == nullptr)) {
        GRAPHIC_LOGE("File is not opened, call Open first\n");
        return RetCode::FAIL;
    }

    // The error handler must be installed before any libjpeg call, and the
    // setjmp zone must live in a frame that is still alive when it fires.
    session->cinfo.err = jpeg_std_error(&(session->jerr.pub));
    session->jerr.pub.error_exit = JpegErrorExit;
    if (setjmp(session->jerr.setjmpBuffer) != 0) {
        if (session->inited) {
            jpeg_destroy_decompress(&(session->cinfo));
            session->inited = false;
        }
        GRAPHIC_LOGE("Read jpeg header failed\n");
        return RetCode::FAIL;
    }

    jpeg_create_decompress(&(session->cinfo));
    session->inited = true;
    jpeg_stdio_src(&(session->cinfo), session->fp);
    if (jpeg_read_header(&(session->cinfo), TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&(session->cinfo));
        session->inited = false;
        GRAPHIC_LOGE("Read jpeg header failed\n");
        return RetCode::FAIL;
    }
    // Force 3-component RGB output whatever the source color space is, so
    // that the BGRA conversion below stays in bounds.
    session->cinfo.out_color_space = JCS_RGB;
    if (jpeg_start_decompress(&(session->cinfo)) != TRUE) {
        jpeg_destroy_decompress(&(session->cinfo));
        session->inited = false;
        GRAPHIC_LOGE("Start jpeg decompress failed\n");
        return RetCode::FAIL;
    }

    return RetCode::OK;
}

void JpegImgDecoder::DestroyDecoder(ImgResDsc& dsc)
{
    JpegSession* session = static_cast<JpegSession*>(dsc.session);
    if (session == nullptr) {
        return;
    }

    // No jpeg_finish_decompress here: with a truncated input it raises an
    // error while no setjmp zone is alive, which would longjmp into a dead
    // frame. jpeg_destroy_decompress alone releases all decoder resources.
    if (session->inited) {
        jpeg_destroy_decompress(&(session->cinfo));
        session->inited = false;
    }

    if (session->fp != nullptr) {
        fclose(session->fp);
        session->fp = nullptr;
    }
}

RetCode JpegImgDecoder::GetHeader(ImgResDsc& dsc)
{
    if (CreateDecoder(dsc) != RetCode::OK) {
        GRAPHIC_LOGE("Decoder is created failed\n");
        return RetCode::FAIL;
    }

    JpegSession* session = static_cast<JpegSession*>(dsc.session);
    dsc.imgInfo.header.width = session->cinfo.output_width;
    dsc.imgInfo.header.height = session->cinfo.output_height;
    dsc.imgInfo.header.colorMode = ARGB8888;

    return RetCode::OK;
}

RetCode JpegImgDecoder::ReadToCache(ImgResDsc& dsc)
{
    JpegSession* session = static_cast<JpegSession*>(dsc.session);
    if ((session == nullptr) || (!session->inited)) {
        GRAPHIC_LOGE("Decoder is not created, call GetHeader first\n");
        return RetCode::FAIL;
    }

    // Re-arm error handling in this still-alive frame: the one installed by
    // CreateDecoder returned long ago. Left-over resources are released by Close.
    if (setjmp(session->jerr.setjmpBuffer) != 0) {
        GRAPHIC_LOGE("Decode jpeg image failed\n");
        return RetCode::FAIL;
    }

    if (session->cinfo.output_components != 3) { // 3: RGB components per pixel
        GRAPHIC_LOGE("Unsupported jpeg pixel format\n");
        return RetCode::FAIL;
    }

    uint8_t pixelByteSize = DrawUtils::GetPxSizeByColorMode(ARGB8888) >> 3; // 3: Shift right 3 bits
    uint16_t width = session->cinfo.output_width;
    uint16_t height = session->cinfo.output_height;
    uint32_t dataSize = width * height * pixelByteSize;
    uint16_t rowStride = session->cinfo.output_width * session->cinfo.output_components;
    JSAMPARRAY buffer = (*session->cinfo.mem->alloc_sarray)(
        reinterpret_cast<j_common_ptr>(&(session->cinfo)), JPOOL_IMAGE, rowStride, 1); // 1: one-row-high array
    if (buffer == nullptr) {
        GRAPHIC_LOGE("Alloc jpeg line buffer failed\n");
        return RetCode::FAIL;
    }

    dsc.imgInfo.dataSize = dataSize;
    uint8_t* srcData = static_cast<uint8_t*>(ImageCacheMalloc(dsc.imgInfo));
    dsc.imgInfo.data = srcData;
    if (srcData == nullptr) {
        GRAPHIC_LOGE("Alloc image cache failed\n");
        return RetCode::FAIL;
    }

    uint32_t n = 0;
    while (session->cinfo.output_scanline < session->cinfo.output_height) {
        if (jpeg_read_scanlines(&(session->cinfo), buffer, 1) != 1) { // 1: read one line each time
            GRAPHIC_LOGE("Decode jpeg image failed\n");
            return RetCode::FAIL;
        }
        for (uint16_t x = 0; x < width * 3; x += 3) { // 3: color components per pixel
            srcData[n++] = buffer[0][x + 2];          // 2: B channel
            srcData[n++] = buffer[0][x + 1];          // 1: G channel
            srcData[n++] = buffer[0][x + 0];          // 0: R channel
            srcData[n++] = 255;                       // 255: set alpha channel
        }
    }
    DestroyDecoder(dsc);
    dsc.inCache = true;

    return RetCode::OK;
}
} // namespace OHOS

#endif // ENABLE_JPEG
