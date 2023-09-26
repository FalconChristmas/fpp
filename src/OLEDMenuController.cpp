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

#include "log.h"
#include "commands/Commands.h"
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <cstring>
#include <fcntl.h>
#include <map>
#include <string>
#include <thread>
#include <unistd.h>
#include <utility>

#include "OLEDMenuController.h"
#include "oled/FPPStatusOLEDPage.h"
#include "oled/OLEDPages.h"

OLEDMenuController OLEDMenuController::INSTANCE;

std::map<std::string, OLEDMenuController::OLEDMenuControlOption> OLEDMenuController::s_mapStringValues = { { "Up", OLEDMenuController::OLED_M_CMD_UP },
                                                                                                           { "Down", OLEDMenuController::OLED_M_CMD_DOWN },
                                                                                                           { "Back", OLEDMenuController::OLED_M_CMD_BACK },
                                                                                                           { "Right", OLEDMenuController::OLED_M_CMD_RIGHT },
                                                                                                           { "Enter", OLEDMenuController::OLED_M_CMD_ENTER },
                                                                                                           { "Test", OLEDMenuController::OLED_M_CMD_TEST },
                                                                                                           { "Mode", OLEDMenuController::OLED_M_CMD_MODE },
                                                                                                           { "TestMultiSync", OLEDMenuController::OLED_M_CMD_TESTMUTLISYNC } };
struct DisplayStatus {
    unsigned int i2cBus;                         // [0] i2c bus #
    volatile unsigned int displayOn;             // [1] display on status
    volatile unsigned int forceOff;              // [2] force off flag
    volatile unsigned int actionToProcess;       // [3] flag to alert an action needs to be processed
    volatile unsigned int InputCmdUp;            // [4] Menu Control: Up
    volatile unsigned int InputCmdDown;          // [5] Menu Control: Down
    volatile unsigned int InputCmdBack;          // [6] Menu Control: Back
    volatile unsigned int InputCmdRight;         // [7] Menu Control: Right
    volatile unsigned int InputCmdEnter;         // [8] Menu Control: Enter
    volatile unsigned int InputCmdTest;          // [9] Menu Control: Test
    volatile unsigned int InputCmdMode;          // [10] Menu Control: Mode
    volatile unsigned int InputCmdTestMultisync; // [11] Menu Control: Test Multisync
};

static DisplayStatus* currentStatus;

void OLEDMenuController::setFPPOLEDMemoryBit(int memberToSet, bool bitvalue) {
    // Set status of OLED to Shared memory
    int smfd = shm_open("fppoled", O_CREAT | O_RDWR, 0);
    ftruncate(smfd, 1024);
    currentStatus = (DisplayStatus*)mmap(0, 1024, PROT_WRITE | PROT_READ, MAP_SHARED, smfd, 0);
    close(smfd);
    switch (memberToSet) {
    case 0:
        currentStatus->i2cBus = bitvalue;
        break;
    case 1:
        currentStatus->displayOn = bitvalue;
        break;
    case 2:
        currentStatus->forceOff = bitvalue;
        break;
    case 3:
        currentStatus->actionToProcess = bitvalue;
        break;
    case 4:
        // ensure only 1 action set at a time
        if (currentStatus->actionToProcess == true) {
            break;
        }
        currentStatus->InputCmdUp = bitvalue;
        break;
    case 5:
        if (currentStatus->actionToProcess == true) {
            break;
        }
        currentStatus->InputCmdDown = bitvalue;
        break;
    case 6:
        if (currentStatus->actionToProcess == true) {
            break;
        }
        currentStatus->InputCmdBack = bitvalue;
        break;
    case 7:
        if (currentStatus->actionToProcess == true) {
            break;
        }
        currentStatus->InputCmdRight = bitvalue;
        break;
    case 8:
        if (currentStatus->actionToProcess == true) {
            break;
        }
        currentStatus->InputCmdEnter = bitvalue;
        break;
    case 9:
        if (currentStatus->actionToProcess == true) {
            break;
        }
        currentStatus->InputCmdTest = bitvalue;
        break;
    case 10:
        if (currentStatus->actionToProcess == true) {
            break;
        }
        currentStatus->InputCmdMode = bitvalue;
        break;
    case 11:
        if (currentStatus->actionToProcess == true) {
            break;
        }
        currentStatus->InputCmdTestMultisync = bitvalue;
        break;
    }
}

void OLEDMenuController::setOLEDMemoryAction(int i) {
    i = i + 3;                    // add offset on numbering to convert from action to memory bit position
    setFPPOLEDMemoryBit(i, true); // Set the correct action bit
    setFPPOLEDMemoryBit(3, true); // set the actionToProcess bit
}

// Incoming actions numbers are different to memory bits array positions used to store in shared memory
void OLEDMenuController::SendActionToOLEDProcess(int i) {
    setOLEDMemoryAction(i);
}

int main() {
    // Create instance
    // OLEDMenuController OLEDMenuController;
}
