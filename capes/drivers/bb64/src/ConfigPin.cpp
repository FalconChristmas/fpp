#include <sys/fcntl.h>
#include <sys/mman.h>
#include <list>
#include <map>
#include <string.h>
#include <string>
#include <unistd.h>

#include "Ball.h"
#include "Pin.h"

constexpr uint64_t MEMLOCATIONS[] = { 0x4084000, 0xf4000, 0x0 };
constexpr uint64_t MAX_OFFSET = 0x260;

extern void InitPocketBeagle2();

extern "C" {
int main(int argc, char const* argv[]) {
    InitPocketBeagle2();

    int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    int d = 0;
    while (MEMLOCATIONS[d]) {
        uint8_t* gpio_map = (uint8_t*)mmap(
            NULL,                   /* Any address in our space will do */
            MAX_OFFSET,             /* Map length */
            PROT_READ | PROT_WRITE, /* Enable reading & writing */
            MAP_SHARED,             /* Shared with other processes */
            mem_fd,                 /* File to map */
            MEMLOCATIONS[d]         /* Offset to GPIO peripheral */
        );
        Ball::setDomainAddress(d, gpio_map);
        ++d;
    }
    close(mem_fd);

    std::string arg1(argv[1]);
    if (arg1 == "-q") {
        if (argc > 2) {
            Pin::getPin(argv[2]).query();
        } else {
            for (auto& p : Pin::getAllPins()) {
                printf("%s: \n", p.first.c_str());
                p.second.query();
            }
        }
    } else if (arg1 == "-s") {
        Pin::getPin(argv[2]).setMode(argv[3]);
    } else if (arg1 == "-l") {
        if (argc > 2) {
            Pin::getPin(argv[2]).listModes();
        } else {
            for (auto& p : Pin::getAllPins()) {
                printf("%s: ", p.first.c_str());
                p.second.listModes();
            }
        }
    }
    return 0;
}
}
