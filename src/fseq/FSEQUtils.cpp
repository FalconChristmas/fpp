
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <libgen.h>

#include <string>
#include <vector>

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
    printf("   -m FSEQFILE       - FSEQ to merge onto the input\n");
    printf("   -f #              - FSEQ Version\n");
    printf("   -c (none|zstd|zlib) - Compession type\n");
    printf("   -l #              - Compession level (-1 for default)\n");
    printf("   -r (#-# | #+#)    - Channel Range.  Use - to separate start/end channel\n");
    printf("                            Use + to separate start channel + num channels\n");
    printf("   -n                - No Sparse. -r will only read the range, but the resulting fseq is not sparse.\n");
    printf("   -j                - Output the fseq file metadata to json\n");
    printf("   -h                - This help output\n");
}
const char *outputFilename = nullptr;
static std::vector<std::string> mergeFseqs;
static int fseqVersion = 2;
static int compressionLevel = -1;
static bool verbose = false;
static std::vector<std::pair<uint32_t, uint32_t>> ranges;
static bool sparse = true;
static bool json = false;
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
        
        c = getopt_long(argc, argv, "c:l:o:f:r:m:hjVvn", long_options, &option_index);
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
            case 'j':
                json = true;
                break;
            case 'v':
                verbose = true;
                break;
            case 'c':
                if (strcmp(optarg, "none") == 0) {
                    compressionType = V2FSEQFile::CompressionType::none;
                } else if (strcmp(optarg, "zlib") == 0) {
                    compressionType = V2FSEQFile::CompressionType::zlib;
                } else if (strcmp(optarg, "zstd") == 0) {
                    compressionType = V2FSEQFile::CompressionType::zstd;
                } else {
                    printf("Unknown compression type: %s\n", optarg);
                    exit(EXIT_FAILURE);
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
            case 'm':
                mergeFseqs.push_back(optarg);
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
        
        if (json) {
            /*
             getNumFrames() const { return m_seqNumFrames; }
             int           getStepTime() const { return m_seqStepTime; }
             uint32_t      getChannelCount() const  { return m_seqChannelCount; }
             uint64_t      getUniqueId() const { return m_uniqueId; }
             const std::string& getFilename() const { return m_filename; }
             */
            char buf[256];
            strcpy(buf, src->getFilename().c_str());
            printf("{\"Name\": \"%s\", \"Version\": \"%d.%d\", \"ID\": \"%" PRIu64 "\", \"StepTime\": %d, \"NumFrames\": %d, \"MaxChannel\": %d, \"ChannelCount\": %d",
                   basename(buf),
                   src->getVersionMajor(), src->getVersionMinor(),
                   src->getUniqueId(),
                   src->getStepTime(),
                   src->getNumFrames(),
                   src->getMaxChannel(),
                   src->getChannelCount()
                   );
            if (!src->getVariableHeaders().empty()) {
                printf(", \"variableHeaders\": {");
                bool first = true;
                for (auto &head : src->getVariableHeaders()) {
                    if (head.code[0] > 32 && head.code[0] <= 127
                        && head.code[1] > 32 && head.code[1] <= 127) {
                        
                        bool allAscii = true;
                        for (auto b : head.data) {
                            if (b && (b < 32 || b > 127)) {
                                allAscii = false;
                            }
                        }
                        if (allAscii) {
                            if (!first) {
                                printf(", ");
                            } else {
                                first = false;
                            }
                            printf("\"%c%c\": \"%s\"", head.code[0], head.code[1], &head.data[0]);
                        }
                    }
                }
                printf("}");
            }
            if (src->getVersionMajor() >= 2) {
                V2FSEQFile *f = (V2FSEQFile*)src;
                if (!f->m_sparseRanges.empty()) {
                    printf(", \"Ranges\": [");
                    int first = true;
                    for (auto &a : f->m_sparseRanges) {
                        if (!first) {
                            printf(",");
                        }
                        first = false;
                        printf("{\"Start\": %d, \"Length\": %d}", a.first, a.second);
                    }
                    printf("]");
                }
            }
            printf("}\n");
        } else {
            std::vector<FSEQFile *> merges;
            for (auto &f : mergeFseqs) {
                FSEQFile *src = FSEQFile::openFSEQFile(f);
                if (src) {
                    merges.push_back(src);
                }
            }

            if (outputFilename == nullptr) {
                if (!verbose) {
                    printf("No output file defined!\n");
                    printf("Use -v to enable verbose output printing for file information.\n");
                }
                delete src;
                return 0;
            }
            
            FSEQFile *dest = FSEQFile::createFSEQFile(outputFilename,
                                                      fseqVersion,
                                                      compressionType,
                                                      compressionLevel);
            if (dest == nullptr) {
                printf("Failed to create FSEQ file (returned nullptr)!\n");
                delete src;
                return 1;
            }
            
            if (ranges.empty()) {
                ranges.push_back(std::pair<uint32_t, uint32_t>(0, 999999999));
            } else if (fseqVersion == 2 && sparse) {
                V2FSEQFile *f = (V2FSEQFile*)dest;
                f->m_sparseRanges = ranges;
            }
            src->prepareRead(ranges);

            dest->initializeFromFSEQ(*src);
            dest->writeHeader();
            
            uint8_t *data = (uint8_t *)malloc(8024*1024);
            uint8_t *mergedata = (uint8_t *)malloc(8024*1024);
            memset(mergedata, 0, 8024*1024);
            for (int x = 0; x < src->getNumFrames(); x++) {
                FSEQFile::FrameData *fdata = src->getFrame(x);
                fdata->readFrame(data, 8024*1024);
                delete fdata;
                
                for (auto m : merges) {
                    FSEQFile::FrameData *fdata = m->getFrame(x);
                    fdata->readFrame(mergedata, 8024*1024);
                    delete fdata;
                    for (int y = 0; y < 8024*1024; ++y) {
                        if (mergedata[y]) {
                            data[y] = mergedata[y];
                            mergedata[y] = 0;
                        }
                    }
                }
                dest->addFrame(x, data);
            }
            free(data);
            free(mergedata);
            dest->finalize();
            
            if (!strcmp(outputFilename, "-memory-")) {
                printf("size: %d\n", (int)dest->getMemoryBuffer().size());
            }
            
            delete dest;
            for (auto a : merges) {
                delete a;
            }
        }
        delete src;
    }

    
    return 0;
}
