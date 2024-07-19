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

#include "fpp-pch.h"

#include "SocketFrameBuffer.h"
#ifdef USE_FRAMEBUFFER_SOCKET

#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <cstring>
#include <ctime>
#include <errno.h>
#include <fcntl.h>
#include <mutex>
#include <stdlib.h>
#include <string>
#include <thread>
#include <unistd.h>

/*
 *
 */
SocketFrameBuffer::SocketFrameBuffer() {
}

/*
 *
 */
SocketFrameBuffer::~SocketFrameBuffer() {
}

void SocketFrameBuffer::DestroyFrameBuffer(void) {
    if (m_buffer) {
        memset(m_buffer, 0, m_bufferSize);
        munmap(m_buffer, m_bufferSize);
    }
    if (shmemFile != -1) {
        close(shmemFile);
        std::string shmFile = m_device + "-buffer";
        shm_unlink(shmFile.c_str());
        shmemFile = -1;
    }
    FrameBuffer::DestroyFrameBuffer();
}

/*
 *
 */
int SocketFrameBuffer::InitializeFrameBuffer(void) {
    std::string devString = getSetting("framebufferControlSocketPath", "/dev") + "/" + m_device;
    m_pages = 3;
    m_bpp = 32;
    m_rowStride = m_width * 4;
    m_rowPadding = 0;
    m_pageSize = m_width * m_height * 4;
    m_bufferSize = m_pageSize * m_pages;

    m_fbFd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (m_fbFd < 0) {
        LogErr(VB_CHANNELOUT, "Error opening FrameBuffer device: %s\n", devString.c_str());
        return 0;
    }

    std::string shmFile = "/fpp/" + m_device + "-buffer";
    shmemFile = shm_open(shmFile.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if (shmemFile == -1) {
        LogErr(VB_CHANNELOUT, "Error creating shared memory buffer: %s   %s\n", shmFile.c_str(), strerror(errno));
        close(m_fbFd);
        m_fbFd = -1;
        return 0;
    }
    if (ftruncate(shmemFile, m_bufferSize) == -1) {
        close(shmemFile);
        shm_unlink(shmFile.c_str());
        shmemFile = shm_open(shmFile.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        ftruncate(shmemFile, m_bufferSize);
    }
    m_buffer = (uint8_t*)mmap(NULL, m_bufferSize, PROT_READ | PROT_WRITE, MAP_SHARED, shmemFile, 0);
    if (m_buffer == MAP_FAILED) {
        LogErr(VB_CHANNELOUT, "Error mmap buffer: %s    %s\n", shmFile.c_str(), strerror(errno));
        close(m_fbFd);
        m_fbFd = -1;
        close(shmemFile);
        shmemFile = -1;
        shm_unlink(shmFile.c_str());
        m_buffer = nullptr;
        return 0;
    }
    memset(m_buffer, 0, m_bufferSize);
    m_pageBuffers[0] = m_buffer;
    m_pageBuffers[1] = m_buffer + m_pageSize;
    m_pageBuffers[2] = m_buffer + m_pageSize + m_pageSize;

    memset(&dev_address, 0, sizeof(struct sockaddr_un));
    dev_address.sun_family = AF_UNIX;
    strcpy(dev_address.sun_path, devString.c_str());

    targetExists = FileExists(dev_address.sun_path);
    sendFramebufferConfig();
    return 1;
}
void SocketFrameBuffer::sendFramebufferConfig() {
    // CMD 1 : send the w/h/pages so the framebuffer can setup
    uint16_t data[4] = { 1, (uint16_t)m_width, (uint16_t)m_height, (uint16_t)m_pages };

    struct msghdr msg = { 0 };
    char buf[CMSG_SPACE(sizeof(shmemFile))];
    memset(buf, '\0', sizeof(buf));

    struct iovec io = { .iov_base = data, .iov_len = sizeof(data) };

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(shmemFile));

    memmove(CMSG_DATA(cmsg), &shmemFile, sizeof(shmemFile));
    msg.msg_controllen = CMSG_SPACE(sizeof(shmemFile));

    msg.msg_name = &dev_address;
    msg.msg_namelen = sizeof(dev_address);
    sendmsg(m_fbFd, &msg, 0);
}

void SocketFrameBuffer::sendFramebufferFrame() {
    // CMD 2 - sync, param is page#
    bool fe = FileExists(dev_address.sun_path);
    if (!targetExists && fe) {
        sendFramebufferConfig();
        targetExists = true;
    } else if (!fe) {
        targetExists = false;
    }
    if (targetExists) {
        uint16_t data[2] = { 2, (uint16_t)m_cPage };
        sendto(m_fbFd, data, sizeof(data), 0, (struct sockaddr*)&dev_address, sizeof(dev_address));
    }
}

void SocketFrameBuffer::SyncLoop() {
    while (m_runLoop) {
        if (m_dirtyPages[m_cPage]) {
            sendFramebufferFrame();

            m_dirtyPages[m_cPage] = 0;
            NextPage();
        }

        usleep(25000);
    }
}

void SocketFrameBuffer::SyncDisplay(bool pageChanged) {
    if (!pageChanged)
        return;

    if (m_pages == 1)
        return;
    sendFramebufferFrame();
}

#endif