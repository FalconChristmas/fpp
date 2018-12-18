
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "fppversion.h"
#include "log.h"

#include "FSEQFile.h"

void usage(char *appname) {
    printf("Usage: %s [OPTIONS] FileName.fseq\n", appname);
    printf("\n");
    printf("  Options:\n");
    printf("   -V                - Print version information\n");
    printf("   -v                - verbose\n");
    printf("   -o OUTPUTFILE     - Filename for Output FSEQ\n");
    printf("   -f #              - FSEQ Version\n");
    printf("   -c (none|zstd|zlib) - Compession type\n");
    printf("   -l #              - Compession level (-1 for default)\n");
    printf("   -r (#-# | #+#)    - Channel Range.  Use - to separate start/end channel\n");
    printf("                            Use + to separate start channel + num channels\n");
    printf("   -n                - No Sparse. -r will only read the range, but the resulting fseq is not sparse.\n");
    printf("   -h                - This help output\n");
}
const char *outputFilename = nullptr;
static int fseqVersion = 2;
static int compressionLevel = -1;
static bool verbose = false;
static std::vector<std::pair<uint32_t, uint32_t>> ranges;
static bool sparse = true;
static V2FSEQFile::CompressionType compressionType = V2FSEQFile::CompressionType::zstd;

int parseArguments(int argc, char **argv) {
    char *s = NULL;
    int   c;
    
    int this_option_optind = optind;
    while (1) {
        this_option_optind = optind ? optind : 1;
        int option_index = 0;
        static struct option long_options[] = {
            {"help",           no_argument,          0, 'h'},
            {"output",         required_argument,    0, 'o'},
            {0,                0,                    0, 0}
        };
        
        c = getopt_long(argc, argv, "c:l:o:f:r:hVvn", long_options, &option_index);
        if (c == -1) {
            break;
        }
        
        switch (c) {
            case 'r': {
                    char *end = optarg;
                    int startc = strtol(optarg, &end, 10);
                    int r = *end == '-';
                    end++;
                    int endc = strtol(end, &end, 10);
                    if (r) {
                        ranges.push_back(std::pair<uint32_t, uint32_t>(startc, endc-startc+1));
                    } else {
                        ranges.push_back(std::pair<uint32_t, uint32_t>(startc, endc));
                    }
                }
                break;
            case 'v':
                verbose = true;
                break;
            case 'c':
                if (strcmp(optarg, "none") == 0) {
                    compressionType = V2FSEQFile::CompressionType::none;
                } else if (strcmp(optarg, "zlib") == 0) {
                    compressionType = V2FSEQFile::CompressionType::zlib;
                } else {
                    compressionType = V2FSEQFile::CompressionType::zstd;
                }
                break;
            case 'l':
                compressionLevel = strtol(optarg, NULL, 10);
                break;
            case 'f':
                fseqVersion = strtol(optarg, NULL, 10);
                break;
            case 'o':
                outputFilename = optarg;
                break;
            case 'n':
                sparse = false;
                break;
            case 'V':
                printVersionInfo();
                exit(0);
            case 'h':
                usage(argv[0]);
                exit(EXIT_SUCCESS);
            default:
                usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    return this_option_optind;
}

int main(int argc, char *argv[]) {
    int idx = parseArguments(argc, argv);
    if (verbose) {
        SetLogLevel("debug");
    }
    FSEQFile *src = FSEQFile::openFSEQFile(argv[idx]);
    if (src) {
        
        FSEQFile *dest = FSEQFile::createFSEQFile(outputFilename,
                                                  fseqVersion,
                                                  compressionType,
                                                  compressionLevel);
        if (ranges.empty()) {
            ranges.push_back(std::pair<uint32_t, uint32_t>(0, 999999999));
        } else if (fseqVersion == 2 && sparse) {
            V2FSEQFile *f = (V2FSEQFile*)dest;
            f->m_sparseRanges = ranges;
        }
        src->prepareRead(ranges);

        dest->initializeFromFSEQ(*src);
        dest->writeHeader();
        
        uint8_t data[1024*1024];
        for (int x = 0; x < src->m_seqNumFrames; x++) {
            FrameData *fdata = src->getFrame(x);
            fdata->readFrame(data);
            delete fdata;
            dest->addFrame(x, data);
        }
        dest->finalize();
        delete dest;
        delete src;
    }

    
    return 0;
}
