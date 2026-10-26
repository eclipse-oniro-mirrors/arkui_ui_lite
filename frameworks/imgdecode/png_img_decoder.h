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

#ifndef GRAPHIC_LITE_PNG_IMG_DECODER_H
#define GRAPHIC_LITE_PNG_IMG_DECODER_H

#include "graphic_config.h"

#if defined(ENABLE_PNG) && ENABLE_PNG

#include <cstdio>

#include "file_img_decoder.h"
#include "png.h"

namespace OHOS {
class PngImgDecoder : public FileImgDecoder {
public:
    static PngImgDecoder& GetInstance()
    {
        static PngImgDecoder instance;
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
    // Per-image decoding session, kept in ImgResDsc::session so that the
    // singleton decoder never holds cross-image state.
    struct PngSession {
        FILE* fp;
        png_structp png;
        png_infop info;
    };

    PngImgDecoder() {}
    ~PngImgDecoder() override {}

    RetCode CreateDecoder(ImgResDsc& dsc);
    void DestroyDecoder(ImgResDsc& dsc);
    RetCode UpdateInfo(ImgResDsc& dsc);

    PngImgDecoder(const PngImgDecoder&) = delete;
    PngImgDecoder& operator=(const PngImgDecoder&) = delete;
    PngImgDecoder(PngImgDecoder&&) = delete;
    PngImgDecoder& operator=(PngImgDecoder&&) = delete;
};
} // namespace OHOS

#endif // ENABLE_PNG

#endif
