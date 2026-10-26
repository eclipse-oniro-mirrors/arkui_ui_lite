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

#ifndef GRAPHIC_LITE_JPEG_IMG_DECODER_H
#define GRAPHIC_LITE_JPEG_IMG_DECODER_H

#include "graphic_config.h"

#if defined(ENABLE_JPEG) && ENABLE_JPEG

#include <cstdio>
#include <setjmp.h>

#include "file_img_decoder.h"
#include "jpeglib.h"

namespace OHOS {
class JpegImgDecoder : public FileImgDecoder {
public:
    static JpegImgDecoder& GetInstance()
    {
        static JpegImgDecoder instance;
        return instance;
    }

    RetCode Open(ImgResDsc& dsc) override;

    RetCode Close(ImgResDsc& dsc) override;

    RetCode GetHeader(ImgResDsc& dsc) override;

    RetCode ReadToCache(ImgResDsc& dsc) override;

    RetCode ReadLine(ImgResDsc& dsc, const Point& start, int16_t len, uint8_t* buf) override
    {
        return RetCode::FAIL;
    }

private:
    // Error handler replacing the default one, which would exit() the process.
    struct JpegErrorMgr {
        struct jpeg_error_mgr pub;
        jmp_buf setjmpBuffer;
    };

    // Per-image decoding session, kept in ImgResDsc::session so that the
    // singleton decoder never holds cross-image state.
    struct JpegSession {
        FILE* fp;
        struct jpeg_decompress_struct cinfo;
        JpegErrorMgr jerr;
        bool inited;
    };

    JpegImgDecoder() {}
    ~JpegImgDecoder() override {}

    static void JpegErrorExit(j_common_ptr cinfo);

    RetCode CreateDecoder(ImgResDsc& dsc);

    void DestroyDecoder(ImgResDsc& dsc);

    JpegImgDecoder(const JpegImgDecoder&) = delete;
    JpegImgDecoder& operator=(const JpegImgDecoder&) = delete;
    JpegImgDecoder(JpegImgDecoder&&) = delete;
    JpegImgDecoder& operator=(JpegImgDecoder&&) = delete;
};
} // namespace OHOS

#endif // ENABLE_JPEG

#endif
