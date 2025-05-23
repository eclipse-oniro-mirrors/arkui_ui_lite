/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#ifndef GRAPHIC_LITE_SOCKET_THREAD_H
#define GRAPHIC_LITE_SOCKET_THREAD_H

#include <QThread>
#include <windows.h>

#include "tcp_socket_manager.h"

namespace OHOS {
class SocketThread : public QThread {
    Q_OBJECT
public:
    SocketThread();
    ~SocketThread();

    void run() override;
    void Quit();

    inline TcpSocketClientManager* GetClientManager()
    {
        return clientManager_;
    };

private:
    volatile bool taskQuitQry = false;
    TcpSocketClientManager* clientManager_;
};
} // namespace OHOS

#endif // GRAPHIC_LITE_SOCKET_THREAD_H