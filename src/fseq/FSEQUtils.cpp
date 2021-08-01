
#include <getopt.h>
#include <inttypes.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <list>
#include <string>
#include <vector>

#include "fppversion.h"
#include "log.h"

#include "FSEQFile.h"

void usage(char* appname) {
    printf("Usage: %s [OPTIONS] FileName.fseq\n", appname);
    printf("\n");
    printf("  Options:\n");
    printf("   -V                - Print version information\n");
    printf("   -v                - verbose\n");
    printf("   -o OUTPUTFILE     - Filename for Output FSEQ\n");
    printf("   -m FSEQFILE       - FSEQ to merge onto the input, ignoring 0\n");
    printf("   -M[ FSEQFILE      - FSEQ to merge onto the input, copy 0\n");
    printf("   -f #              - FSEQ Version\n");
    printf("   -c (none|zstd|zlib) - Compession type\n");
    printf("   -l #              - Compression level (-99 for default)\n");
    printf("   -r (#-# | #+#)    - Channel Range.  Use - to separate start/end channel\n");
    printf("                            Use + to separate start channel + num channels\n");
    printf("                       If used before first -m/-M argument, sets a sparse range of output\n");
    printf("                       If used after -m/-M argument, sets a range to read from last merged sequence.\n");
    printf("   -n                - No Sparse. -r will only read the range, but the resulting fseq is not sparse.\n");
    printf("   -j                - Output the fseq file metadata to json\n");
    printf("   -h                - This help output\n");
}
const char* outputFilename = nullptr;

class MergeFSEQ {
public:
    MergeFSEQ(const std::string& f, bool cz) :
        filename(f),
        copyZero(cz) {}

    std::string filename;
    std::vector<std::pair<uint32_t, uint32_t>> ranges;
    bool copyZero = false;
    FSEQFile* srcFile = nullptr;
};

static std::list<MergeFSEQ> mergeFseqs;
static int fseqMajVersion = 2;
static int fseqMinVersion = 0;
static int compressionLevel = -99;
static bool verbose = false;
static std::vector<std::pair<uint32_t, uint32_t>> ranges;
static bool sparse = true;
static bool json = false;
static V2FSEQFile::CompressionType compressionType = V2FSEQFile::CompressionType::zstd;

static void parseRanges(std::vector<std::pair<uint32_t, uint32_t>>& ranges, char* rng) {
    char* end = rng;
    int startc = strtol(rng, &end, 10);
    int r = *end == '-';
    end++;
    int endc = strtol(end, &end, 10);
    if (r) {
        ranges.push_back(std::pair<uint32_t, uint32_t>(startc - 1, endc - startc + 1));
    } else {
        ranges.push_back(std::pair<uint32_t, uint32_t>(startc - 1, endc));
    }
}

int parseArguments(int argc, char** argv) {
    char* s = NULL;
    int c;

    int this_option_optind = optind;
    while (1) {
        this_option_optind = optind ? optind : 1;
        int option_index = 0;
        static struct option long_options[] = {
            { "help", no_argument, 0, 'h' },
            { "output", required_argument, 0, 'o' },
            { 0, 0, 0, 0 }
        };

        c = getopt_long(argc, argv, "c:l:o:f:r:m:M:hjVvn", long_options, &option_index);
        if (c == -1) {
            break;
        }

        switch (c) {
        case 'r':
            if (mergeFseqs.empty()) {
                parseRanges(ranges, optarg);
            } else {
                parseRanges(mergeFseqs.back().ranges, optarg);
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
        case 'f': {
            char* next = nullptr;
            fseqMajVersion = strtol(optarg, &next, 10);
            if (*next == '.') {
                next++;
                fseqMinVersion = strtol(next, &next, 10);
            }
        } break;
        case 'o':
            outputFilename = optarg;
            break;
        case 'm':
            mergeFseqs.push_back(MergeFSEQ(optarg, false));
            break;
        case 'M':
            mergeFseqs.push_back(MergeFSEQ(optarg, true));
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

static char* escape(char* buf, const char* data) {
    char* dest = buf;
    while (*data) {
        *dest = *data;
        dest++;
        if (*data == '\\') {
            *dest = '\\';
            dest++;
        }
        data++;
    }
    *dest = 0;
    return buf;
}

int main(int argc, char* argv[]) {
    int idx = parseArguments(argc, argv);
    if (verbose) {
        SetLogLevel("debug");
    } else {
        SetLogFile("stderr", false);
    }
    FSEQFile* src = FSEQFile::openFSEQFile(argv[idx]);
    if (src) {
        if (json) {
            /*
             getNumFrames() const { return m_seqNumFrames; }
             int           getStepTime() const { return m_seqStepTime; }
             uint32_t      getChannelCount() const  { return m_seqChannelCount; }
             uint64_t      getUniqueId() const { return m_uniqueId; }
             const std::string& getFilename() const { return m_filename; }
             */
            char buf[512];
            strcpy(buf, src->getFilename().c_str());
            printf("{\"Name\": \"%s\", \"Version\": \"%d.%d\", \"ID\": \"%" PRIu64 "\", \"StepTime\": %d, \"NumFrames\": %d, \"MaxChannel\": %d, \"ChannelCount\": %d",
                   escape(buf, basename(buf)),
                   src->getVersionMajor(), src->getVersionMinor(),
                   src->getUniqueId(),
                   src->getStepTime(),
                   src->getNumFrames(),
                   src->getMaxChannel(),
                   src->getChannelCount());
            if (!src->getVariableHeaders().empty()) {
                printf(", \"variableHeaders\": {");
                bool first = true;
                for (auto& head : src->getVariableHeaders()) {
                    if (head.code[0] > 32 && head.code[0] <= 127 && head.code[1] > 32 && head.code[1] <= 127) {
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
                            printf("\"%c%c\": \"%s\"", head.code[0], head.code[1], escape(buf, (const char*)&head.data[0]));
                        }
                    }
                }
                printf("}");
            }
            if (src->getVersionMajor() >= 2) {
                V2FSEQFile* f = (V2FSEQFile*)src;
                if (!f->m_sparseRanges.empty()) {
                    printf(", \"Ranges\": [");
                    int first = true;
                    for (auto& a : f->m_sparseRanges) {
                        if (!first) {
                            printf(",");
                        }
                        first = false;
                        printf("{\"Start\": %d, \"Length\": %d}", a.first, a.second);
                    }
                    printf("]");
                }
                printf(", \"CompressionType\": %d", (int)f->m_compressionType);
            }
            printf("}\n");
        } else {
            for (auto& f : mergeFseqs) {
                FSEQFile* src = FSEQFile::openFSEQFile(f.filename);
                if (src) {
                    f.srcFile = src;
                    if (f.ranges.empty()) {
                        f.ranges.push_back(std::pair<uint32_t, uint32_t>(0, 8024 * 1024));
                    }
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

            FSEQFile* dest = FSEQFile::createFSEQFile(outputFilename,
                                                      fseqMajVersion,
                                                      compressionType,
                                                      compressionLevel);
            if (dest == nullptr) {
                printf("Failed to create FSEQ file (returned nullptr)!\n");
                delete src;
                return 1;
            }
            dest->enableMinorVersionFeatures(fseqMinVersion);

            if (ranges.empty()) {
                ranges.push_back(std::pair<uint32_t, uint32_t>(0, 999999999));
            } else if (fseqMajVersion == 2 && sparse) {
                V2FSEQFile* f = (V2FSEQFile*)dest;
                f->m_sparseRanges = ranges;
            }
            src->prepareRead(ranges);

            dest->initializeFromFSEQ(*src);
            dest->writeHeader();

            uint8_t* data = (uint8_t*)malloc(8024 * 1024);
            uint8_t* mergedata = (uint8_t*)malloc(8024 * 1024);
            memset(mergedata, 0, 8024 * 1024);
            for (int x = 0; x < src->getNumFrames(); x++) {
                FSEQFile::FrameData* fdata = src->getFrame(x);
                fdata->readFrame(data, 8024 * 1024);
                delete fdata;

                for (auto& m : mergeFseqs) {
                    if (m.srcFile) {
                        FSEQFile::FrameData* fdata = m.srcFile->getFrame(x);
                        fdata->readFrame(mergedata, 8024 * 1024);
                        delete fdata;
                        for (auto& r : m.ranges) {
                            for (int y = 0, idx = r.first; y < r.second; ++y, ++idx) {
                                if (mergedata[idx] || m.copyZero) {
                                    data[idx] = mergedata[idx];
                                    mergedata[idx] = 0;
                                }
                            }
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
            for (auto& a : mergeFseqs) {
                if (a.srcFile) {
                    delete a.srcFile;
                }
            }
        }
        delete src;
    }

    return 0;
}
