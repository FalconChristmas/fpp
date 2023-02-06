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

#include <sys/time.h>
#include <sys/types.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <time.h>
#include <unistd.h>

#include "E131.h"
#include "FPD.h"
#include "Sequence.h"
#include "common.h"
#include "falcon.h"
#include "log.h"
#include "settings.h"

#include "util/SPIUtils.h"

#define MAX_PIXELNET_DMX_PORTS 12
#define PIXELNET_DMX_DATA_SIZE 32768
#define PIXELNET_HEADER_SIZE 6
#define PIXELNET_DMX_BUF_SIZE (PIXELNET_DMX_DATA_SIZE + PIXELNET_HEADER_SIZE)

#define PIXELNET_DMX_COMMAND_CONFIG 0
#define PIXELNET_DMX_COMMAND_DATA 0xFF

typedef struct fpdPrivData {
    unsigned char inBuf[PIXELNET_DMX_DATA_SIZE];
    unsigned char outBuf[PIXELNET_DMX_BUF_SIZE];

    int threadIsRunning;
    int runThread;
    int dataWaiting;

    pthread_t processThreadID;
    pthread_mutex_t bufLock;
    pthread_mutex_t sendLock;
    pthread_cond_t sendCond;
} FPDPrivData;

typedef struct {
    char active;
    char type;
    int startChannel;
} PixelnetDMXentry;

extern SPIUtils* falconSpi;

pthread_t pixelnetDMXthread;
unsigned char PixelnetDMXcontrolHeader[] = { 0x55, 0x55, 0x55, 0x55, 0x55, 0xCC };
unsigned char PixelnetDMXdataHeader[] = { 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x55 };

PixelnetDMXentry pixelnetDMX[MAX_PIXELNET_DMX_PORTS];
int pixelnetDMXcount = 0;
int pixelnetDMXactive = 0;

/////////////////////////////////////////////////////////////////////////////
// Prototypes for some functions below
int FPD_StartOutputThread(void* data);
int FPD_StopOutputThread(void* data);

/////////////////////////////////////////////////////////////////////////////

/*
 *
 */
void FPD_Dump(FPDPrivData* privData) {
    LogDebug(VB_CHANNELOUT, "  privData: %p\n", privData);

    if (!privData)
        return;

    LogDebug(VB_CHANNELOUT, "    threadIsRunning: %d\n", privData->threadIsRunning);
    LogDebug(VB_CHANNELOUT, "    runThread      : %d\n", privData->runThread);
}

/*
 *
 */
int SendOutputBuffer(FPDPrivData* privData) {
    LogDebug(VB_CHANNELDATA, "SendOutputBuffer()\n");

    int i;
    unsigned char* c = privData->outBuf + PIXELNET_HEADER_SIZE;

    memcpy(privData->outBuf, PixelnetDMXdataHeader, PIXELNET_HEADER_SIZE);

    pthread_mutex_lock(&privData->bufLock);
    memcpy(c, privData->inBuf, PIXELNET_DMX_DATA_SIZE);
    privData->dataWaiting = 0;
    pthread_mutex_unlock(&privData->bufLock);

    for (i = 0; i < PIXELNET_DMX_DATA_SIZE; i++, c++) {
        if (*c == 170) {
            *c = 171;
        }
    }

    if (WillLog(LOG_EXCESSIVE, VB_CHANNELDATA))
        HexDump("FPD Channel Header & Data", privData->outBuf, 256, VB_CHANNELDATA);

    i = -1;
    if (falconSpi) {
        i = falconSpi->xfer(privData->outBuf, nullptr, PIXELNET_DMX_BUF_SIZE);
    }
    if (i != PIXELNET_DMX_BUF_SIZE) {
        LogErr(VB_CHANNELOUT, "Error: SPI->xfer returned %d, expecting %d\n", i, PIXELNET_DMX_BUF_SIZE);
        return 0;
    }

    return 1;
}

/*
 *
 */
void* RunFPDOutputThread(void* data) {
    LogDebug(VB_CHANNELOUT, "RunFPDOutputThread()\n");

    long long wakeTime = GetTime();
    struct timeval tv;
    struct timespec ts;

    FPDPrivData* privData = (FPDPrivData*)data;

    privData->threadIsRunning = 1;
    LogDebug(VB_CHANNELOUT, "FPD output thread started\n");

    while (privData->runThread) {
        // Wait for more data
        pthread_mutex_lock(&privData->sendLock);
        LogExcess(VB_CHANNELOUT, "FPD output thread: sent: %lld, elapsed: %lld\n",
                  GetTime(), GetTime() - wakeTime);

        pthread_mutex_lock(&privData->bufLock);
        if (privData->dataWaiting) {
            pthread_mutex_unlock(&privData->bufLock);

            gettimeofday(&tv, NULL);
            ts.tv_sec = tv.tv_sec;
            ts.tv_nsec = (tv.tv_usec + 200000) * 1000;

            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec += 1;
                ts.tv_nsec -= 1000000000;
            }

            pthread_cond_timedwait(&privData->sendCond, &privData->sendLock, &ts);
        } else {
            pthread_mutex_unlock(&privData->bufLock);
            pthread_cond_wait(&privData->sendCond, &privData->sendLock);
        }

        wakeTime = GetTime();
        LogExcess(VB_CHANNELOUT, "FPD output thread: woke: %lld\n", GetTime());
        pthread_mutex_unlock(&privData->sendLock);

        if (!privData->runThread)
            continue;

        // See if there is any data waiting to process or if we timed out
        pthread_mutex_lock(&privData->bufLock);
        if (privData->dataWaiting) {
            pthread_mutex_unlock(&privData->bufLock);

            SendOutputBuffer(privData);
        } else {
            pthread_mutex_unlock(&privData->bufLock);
        }
    }

    LogDebug(VB_CHANNELOUT, "FPD output thread complete\n");
    privData->threadIsRunning = 0;
    return nullptr;
}

/*
 *
 */
void CreatePixelnetDMXfile(const char* file) {
    FILE* fp;
    char settings[1024];
    char command[32];
    int i;
    int startChannel = 1;
    fp = fopen(file, "w");
    if (!fp) {
        LogErr(VB_CHANNELOUT, "Error: Unable to create pixelnet file.\n");
        exit(EXIT_FAILURE);
    }
    LogDebug(VB_CHANNELOUT, "Creating file: %s\n", file);

    bzero(settings, 1024);
    // File Header
    settings[0] = 0x55;
    settings[1] = 0x55;
    settings[2] = 0x55;
    settings[3] = 0x55;
    settings[4] = 0x55;
    settings[5] = 0xCC;
    int index = 6;

    // Set first 8 to Pixelnet
    for (i = 0; i < 8; i++, startChannel += 4096) {
        settings[index++] = 1;                          // Enabled
        settings[index++] = (char)(startChannel % 256); // Start Address LSB
        settings[index++] = (char)(startChannel / 256); // Start Address MSB
        settings[index++] = 0;                          // Type 0=Pixlenet, 1=DMX
    }

    // Set next four to DMX
    for (i = 0, startChannel = 1; i < 4; i++, startChannel += 512) {
        settings[index++] = 1;                          // Enabled
        settings[index++] = (char)(startChannel % 256); // Start Address LSB
        settings[index++] = (char)(startChannel / 256); // Start Address MSB
        settings[index++] = 1;                          // Type 0=Pixlenet, 1=DMX
    }

    fwrite(settings, 1, 1024, fp);
    fclose(fp);
    snprintf(command, sizeof(command), "sudo chmod 775 %s", file);
    system(command);
}

int InitializePixelnetDMX() {
    int err;
    LogInfo(VB_CHANNELOUT, "Initializing SPI for FPD output\n");

    if (!DetectFalconHardware(1)) {
        LogWarn(VB_CHANNELOUT, "Unable to detect attached Falcon "
                               "hardware, setting SPI speed to 8000000.\n");

        falconSpi = new SPIUtils(0, 8000000);
        if (!falconSpi->isOk()) {
            delete falconSpi;
            falconSpi = nullptr;
            LogErr(VB_CHANNELOUT, "Unable to open SPI device\n");
            return 0;
        }

        LogWarn(VB_CHANNELOUT, "Sending FPD v1.0 config\n");
        SendFPDConfig();
    }

    return 1;
}

void SendFPDConfig() {
    int i, index;
    unsigned char bufferPixelnetDMX[PIXELNET_DMX_BUF_SIZE];

    memset(bufferPixelnetDMX, 0, PIXELNET_DMX_BUF_SIZE);
    memcpy(bufferPixelnetDMX, PixelnetDMXcontrolHeader, PIXELNET_HEADER_SIZE);
    index = PIXELNET_HEADER_SIZE;
    for (i = 0; i < pixelnetDMXcount; i++) {
        bufferPixelnetDMX[index++] = pixelnetDMX[i].type;
        bufferPixelnetDMX[index++] = (char)(pixelnetDMX[i].startChannel % 256);
        bufferPixelnetDMX[index++] = (char)(pixelnetDMX[i].startChannel / 256);
    }

    if (WillLog(LOG_DEBUG, VB_CHANNELOUT))
        HexDump("FPD Config Header & Data", bufferPixelnetDMX,
                PIXELNET_HEADER_SIZE + (pixelnetDMXcount * 3), VB_CHANNELOUT);

    i = -1;
    if (falconSpi) {
        i = falconSpi->xfer((uint8_t*)bufferPixelnetDMX, nullptr, PIXELNET_DMX_BUF_SIZE);
    }
    if (i != PIXELNET_DMX_BUF_SIZE)
        LogErr(VB_CHANNELOUT, "Error: SPI->xfer returned %d, expecting %d\n", i, PIXELNET_DMX_BUF_SIZE);

    std::this_thread::sleep_for(std::chrono::microseconds(10000));
}

/*
 *
 */
int FPD_Open(const char* configStr, void** privDataPtr) {
    LogDebug(VB_CHANNELOUT, "FPD_Open()\n");

    if (!FileExists(FPP_FILE_PIXELNET)) {
        LogDebug(VB_CHANNELOUT, "FPD config file does not exist, creating it.\n");
        CreatePixelnetDMXfile(FPP_FILE_PIXELNET.c_str());
    }

    if (!InitializePixelnetDMX())
        return 0;

    FPDPrivData* privData =
        (FPDPrivData*)malloc(sizeof(FPDPrivData));
    if (privData == NULL) {
        LogErr(VB_CHANNELOUT, "Error %d allocating private memory: %s\n",
               errno, strerror(errno));
        return 0;
    }

    bzero(privData, sizeof(FPDPrivData));

    pthread_mutex_init(&privData->bufLock, NULL);
    pthread_mutex_init(&privData->sendLock, NULL);
    pthread_cond_init(&privData->sendCond, NULL);

    FPD_Dump(privData);

    *privDataPtr = privData;

    return 1;
}

/*
 *
 */
int FPD_Close(void* data) {
    LogDebug(VB_CHANNELOUT, "FPD_Close(%p)\n", data);

    FPDPrivData* privData = (FPDPrivData*)data;
    FPD_Dump(privData);

    FPD_StopOutputThread(privData);

    pthread_mutex_destroy(&privData->bufLock);
    pthread_mutex_destroy(&privData->sendLock);
    pthread_cond_destroy(&privData->sendCond);
    return 0;
}

/*
 *
 */
int FPD_IsConfigured(void) {
    LogDebug(VB_CHANNELOUT, "FPD_IsConfigured()\n");

    if (!getSettingInt("FPDEnabled"))
        return 0;

    pixelnetDMXactive = 1;
    return pixelnetDMXactive;
}

/*
 *
 */
int FPD_IsActive(void* data) {
    LogDebug(VB_CHANNELOUT, "FPD_IsActive(%p)\n", data);

    return pixelnetDMXactive;
}

/*
 *
 */
int FPD_SendData(void* data, const char* channelData, int channelCount) {
    LogDebug(VB_CHANNELDATA, "FPD_SendData(%p, %p, %d)\n",
             data, channelData, channelCount);

    FPDPrivData* privData = (FPDPrivData*)data;

    if (channelCount > PIXELNET_DMX_DATA_SIZE) {
        LogErr(VB_CHANNELOUT,
               "FPD_SendData() tried to send %d bytes when max is %d\n",
               channelCount, PIXELNET_DMX_DATA_SIZE);
        return 0;
    }

    // Copy latest data to our input buffer for processing
    pthread_mutex_lock(&privData->bufLock);
    memcpy(privData->inBuf, channelData, channelCount);
    privData->dataWaiting = 1;
    pthread_mutex_unlock(&privData->bufLock);

    if (privData->threadIsRunning)
        pthread_cond_signal(&privData->sendCond);
    else
        SendOutputBuffer(privData);

    return 1;
}

/*
 *
 */
int FPD_MaxChannels(void* data) {
    (void)data;

    return 32768;
}

/*
 *
 */
int FPD_StartOutputThread(void* data) {
    LogDebug(VB_CHANNELOUT, "FPD_StartOutputThread(%p)\n", data);

    FPDPrivData* privData = (FPDPrivData*)data;

    if (privData->processThreadID) {
        LogErr(VB_CHANNELOUT, "ERROR: thread already exists\n");
        return -1;
    }

    privData->runThread = 1;

    int result = pthread_create(&privData->processThreadID, NULL, &RunFPDOutputThread, privData);

    if (result) {
        char msg[256];

        privData->runThread = 0;
        switch (result) {
        case EAGAIN:
            strcpy(msg, "Insufficient Resources");
            break;
        case EINVAL:
            strcpy(msg, "Invalid settings");
            break;
        case EPERM:
            strcpy(msg, "Invalid Permissions");
            break;
        }
        LogErr(VB_CHANNELOUT, "ERROR creating FPD output thread: %s\n", msg);
    }

    while (!privData->threadIsRunning)
        usleep(10000);

    return 0;
}

/*
 *
 */
int FPD_StopOutputThread(void* data) {
    LogDebug(VB_CHANNELOUT, "FPD_StopOutputThread(%p)\n", data);

    FPDPrivData* privData = (FPDPrivData*)data;

    if (!privData->processThreadID)
        return -1;

    privData->runThread = 0;

    pthread_cond_signal(&privData->sendCond);

    int loops = 0;
    // Wait up to 110ms for data to be sent
    while ((privData->dataWaiting) &&
           (privData->threadIsRunning) &&
           (loops++ < 11))
        usleep(10000);

    pthread_mutex_lock(&privData->bufLock);

    if (!privData->processThreadID) {
        pthread_mutex_unlock(&privData->bufLock);
        return -1;
    }

    pthread_join(privData->processThreadID, NULL);
    privData->processThreadID = 0;
    pthread_mutex_unlock(&privData->bufLock);

    return 0;
}

/*
 * Declare our external interface struct
 */
FPPChannelOutput FPDOutput = {
    FPD_MaxChannels,       //maxChannels
    FPD_Open,              //open
    FPD_Close,             //close
    FPD_IsConfigured,      //isConfigured
    FPD_IsActive,          //isActive
    FPD_SendData,          //send
    FPD_StartOutputThread, //startThread
    FPD_StopOutputThread,  //stopThread
};
