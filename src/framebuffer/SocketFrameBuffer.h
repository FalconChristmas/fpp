#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "FrameBuffer.h"

#ifdef USE_FRAMEBUFFER_SOCKET
#include <sys/socket.h>
#include <sys/un.h>

class SocketFrameBuffer : public FrameBuffer {
public:
    SocketFrameBuffer();
    virtual ~SocketFrameBuffer();

    virtual int InitializeFrameBuffer() override;
    virtual void DestroyFrameBuffer() override;
    virtual void SyncLoop() override;
    virtual void SyncDisplay(bool pageChanged = false) override;

    void sendFramebufferConfig();
    void sendFramebufferFrame();

    int shmemFile = -1;
    struct sockaddr_un dev_address;
    bool targetExists = false;
};
#endif
