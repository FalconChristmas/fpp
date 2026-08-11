#include <sys/fcntl.h>
#include <sys/mman.h>
#include <list>
#include <map>
#include <string.h>
#include <string>
#include <unistd.h>

#include "Ball.h"
#include "Pin.h"

extern void InitPocketBeagle1();
extern void InitPocketBeagle2();
extern void InitBeagleBoneBlack();

extern "C" {
int main(int argc, char const* argv[]) {
    FILE* file = fopen("/proc/device-tree/model", "r");
    if (file) {
        char buf[256];
        fgets(buf, 256, file);
        fclose(file);
        if (strcmp(&buf[10], "PocketBeagle") == 0) {
            InitPocketBeagle1();
        } else if (strcmp(buf, "BeagleBoard.org PocketBeagle2") == 0) {
            InitPocketBeagle2();
        } else if (strcmp(buf, "BeagleBoard.org BeaglePlay") == 0) {
            InitPocketBeagle2();
        } else {
            InitBeagleBoneBlack();
        }
    } else {
        InitBeagleBoneBlack();
    }

    std::string arg1;
    if (argc > 1) {
        arg1 = argv[1];
    }
    // Every path below reports success/failure through the exit status.  Callers
    // -- notably FPP's CapeUtils::ConfigurePin() -- branch on it to decide whether
    // the mux actually changed, so "printed a complaint and exited 0" is not an
    // option: an unknown pin or mode has to come back non-zero.
    if (arg1 == "-q") {
        if (argc > 2) {
            if (!Pin::hasPin(argv[2])) {
                printf("Unknown pin: %s\n", argv[2]);
                return 1;
            }
            Pin::getPin(argv[2]).query();
        } else {
            for (auto& p : Pin::getAllPins()) {
                printf("%s: \n", p.first.c_str());
                p.second.query();
            }
        }
    } else if (arg1 == "-s") {
        if (argc <= 3) {
            printf("Usage: %s -s <pin> <mode>\n", argv[0]);
            return 1;
        }
        if (!Pin::hasPin(argv[2])) {
            printf("Unknown pin: %s\n", argv[2]);
            return 1;
        }
        if (!Pin::getPin(argv[2]).setMode(argv[3])) {
            return 1;
        }
    } else if (arg1 == "-l") {
        if (argc > 2) {
            if (!Pin::hasPin(argv[2])) {
                printf("Unknown pin: %s\n", argv[2]);
                return 1;
            }
            Pin::getPin(argv[2]).listModes();
        } else {
            for (auto& p : Pin::getAllPins()) {
                printf("%s: ", p.first.c_str());
                p.second.listModes();
            }
        }
    } else {
        printf("Usage: %s -q <pin> | -s <pin> <mode> | -l <pin>\n", argv[0]);
        printf("       %s -q : query all pins\n", argv[0]);
        printf("       %s -q <pin> : query pin\n", argv[0]);
        printf("       %s -s <pin> <mode> : set pin mode\n", argv[0]);
        printf("       %s -l <pin> : list modes for pin\n", argv[0]);
        return 1;
    }
    return 0;
}
}
