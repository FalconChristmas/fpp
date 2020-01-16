// This #define must be before any #include's
#define _FILE_OFFSET_BITS 64
#define __STDC_FORMAT_MACROS


#include <vector>
#include <cstring>
#include <memory>

#include <atomic>
#include <thread>
#include <mutex>
#include <map>
#include <list>
#include <condition_variable>

#include <chrono>
using namespace std::literals;
using namespace std::chrono_literals;
using namespace std::literals::chrono_literals;

#include <stdio.h>
#include <inttypes.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>


#ifdef _MSC_VER
#include <wx/wx.h>
int gettimeofday(struct timeval * tp, struct timezone * tzp)
{
    // Note: some broken versions only have 8 trailing zero's, the correct epoch has 9 trailing zero's
    // This magic number is the number of 100 nanosecond intervals since January 1, 1601 (UTC)
    // until 00:00:00 January 1, 1970
    static const uint64_t EPOCH = ((uint64_t)116444736000000000ULL);

    SYSTEMTIME  system_time;
    FILETIME    file_time;
    uint64_t    time;

    GetSystemTime(&system_time);
    SystemTimeToFileTime(&system_time, &file_time);
    time = ((uint64_t)file_time.dwLowDateTime);
    time += ((uint64_t)file_time.dwHighDateTime) << 32;

    tp->tv_sec = (long)((time - EPOCH) / 10000000L);
    tp->tv_usec = (long)(system_time.wMilliseconds * 1000);
    return 0;
}
#define ftello _ftelli64
#define fseeko _fseeki64

#else
#include <sys/time.h>
#include <unistd.h>
#endif

#include "FSEQFile.h"

#if defined(PLATFORM_OSX)
#define PLATFORM_UNKNOWN
#endif

#if defined(PLATFORM_PI) || defined(PLATFORM_BBB) || defined(PLATFORM_ODROID) || defined(PLATFORM_ORANGEPI) || defined(PLATFORM_UNKNOWN) || defined(PLATFORM_DOCKER)
//for FPP, use FPP logging
#include "log.h"
#include "Warnings.h"
inline void AddSlowStorageWarning() {
    WarningHolder::AddWarningTimeout("FSEQ Data Block not available - Likely slow storage", 90);
}
#else
//compiling within xLights, use log4cpp
#define PLATFORM_UNKNOWN
#include <log4cpp/Category.hh>
template<typename... Args> static void LogErr(int i, const char *fmt, Args... args) {
    static log4cpp::Category &fseq_logger_base = log4cpp::Category::getInstance(std::string("log_base"));
    char buf[256];
    const char *nfmt = fmt;
    if (fmt[strlen(fmt) - 1] == '\n') {
        strcpy(buf, fmt);
        buf[strlen(fmt) - 1] = 0;
        nfmt = buf;
    }
    fseq_logger_base.error(nfmt, args...);
}
template<typename... Args> static void LogInfo(int i, const char *fmt, Args... args) {
    static log4cpp::Category &fseq_logger_base = log4cpp::Category::getInstance(std::string("log_base"));
    char buf[256];
    const char *nfmt = fmt;
    if (fmt[strlen(fmt) - 1] == '\n') {
        strcpy(buf, fmt);
        buf[strlen(fmt) - 1] = 0;
        nfmt = buf;
    }
    fseq_logger_base.info(nfmt, args...);
}
template<typename... Args> static void LogDebug(int i, const char *fmt, Args... args) {
    static log4cpp::Category &fseq_logger_base = log4cpp::Category::getInstance(std::string("log_base"));
    char buf[256];
    const char *nfmt = fmt;
    if (fmt[strlen(fmt) - 1] == '\n') {
        strcpy(buf, fmt);
        buf[strlen(fmt) - 1] = 0;
        nfmt = buf;
    }
    fseq_logger_base.debug(nfmt, args...);
}
inline void AddSlowStorageWarning() {
}

#define VB_SEQUENCE 1
#define VB_ALL 0
#endif


#ifndef NO_ZSTD
#include <zstd.h>
#endif
#ifndef NO_ZLIB
#include <zlib.h>
#endif

using FrameData = FSEQFile::FrameData;

inline void DumpHeader(const char *title, unsigned char data[], int len) {
    int x = 0;
    char tmpStr[128];

    sprintf( tmpStr, "%s: (%d bytes)\n", title, len);
    LogInfo(VB_ALL, tmpStr);

    for (int y = 0; y < len; y++) {
        if ( x == 0 ) {
            sprintf( tmpStr, "%06x: ", y);
        }
        sprintf( tmpStr + strlen(tmpStr), "%02x ", (int)(data[y] & 0xFF) );
        x++;
        if (x == 16) {
            x = 0;
            sprintf( tmpStr + strlen(tmpStr), "\n" );
            LogInfo(VB_ALL, tmpStr);
        }
    }
    if (x != 0) {
        sprintf( tmpStr + strlen(tmpStr), "\n" );
        LogInfo(VB_ALL, tmpStr);
    }
}

inline uint64_t GetTime(void) {
    struct timeval now_tv;
    gettimeofday(&now_tv, NULL);
    return now_tv.tv_sec * 1000000LL + now_tv.tv_usec;
}

inline long long GetTimeMS(void) {
    struct timeval now_tv;
    gettimeofday(&now_tv, NULL);
    return now_tv.tv_sec * 1000LL + now_tv.tv_usec / 1000;
}
inline long roundTo4(long i) {
    long remainder = i % 4;
    if (remainder == 0) {
        return i;
    }
    return i + 4 - remainder;
}

inline uint32_t read4ByteUInt(const uint8_t* data) {
    uint32_t r = (data[0])
        + (data[1] << 8)
        + (data[2] << 16)
        + (data[3] << 24);
    return r;
}
inline uint32_t read3ByteUInt(const uint8_t* data) {
    uint32_t r = (data[0])
        + (data[1] << 8)
        + (data[2] << 16);
    return r;
}
inline uint32_t read2ByteUInt(const uint8_t* data) {
    uint32_t r = (data[0])
        + (data[1] << 8);
    return r;
}
inline void write2ByteUInt(uint8_t* data, uint32_t v) {
    data[0] = (uint8_t)(v & 0xFF);
    data[1] = (uint8_t)((v >> 8) & 0xFF);
}
inline void write3ByteUInt(uint8_t* data, uint32_t v) {
    data[0] = (uint8_t)(v & 0xFF);
    data[1] = (uint8_t)((v >> 8) & 0xFF);
    data[2] = (uint8_t)((v >> 16) & 0xFF);
}
inline void write4ByteUInt(uint8_t* data, uint32_t v) {
    data[0] = (uint8_t)(v & 0xFF);
    data[1] = (uint8_t)((v >> 8) & 0xFF);
    data[2] = (uint8_t)((v >> 16) & 0xFF);
    data[3] = (uint8_t)((v >> 24) & 0xFF);
}

FSEQFile* FSEQFile::openFSEQFile(const std::string &fn) {

    FILE *seqFile = fopen((const char *)fn.c_str(), "rb");
    if (seqFile == NULL) {
        LogErr(VB_SEQUENCE, "Error opening sequence file: %s. fopen returned NULL\n",
               fn.c_str());
        return nullptr;
    }

    fseeko(seqFile, 0L, SEEK_SET);
    unsigned char tmpData[48];
    int bytesRead = fread(tmpData, 1, 48, seqFile);
#ifndef PLATFORM_UNKNOWN
    posix_fadvise(fileno(seqFile), 0, 0, POSIX_FADV_SEQUENTIAL);
    posix_fadvise(fileno(seqFile), 0, 1024*1024, POSIX_FADV_WILLNEED);
#endif

    if ((bytesRead < 4)
        || (tmpData[0] != 'P' && tmpData[0] != 'F' && tmpData[0] != 'E')
        || tmpData[1] != 'S'
        || tmpData[2] != 'E'
        || tmpData[3] != 'Q') {
        LogErr(VB_SEQUENCE, "Error opening sequence file: %s. Incorrect File Format header: '%s', bytesRead: %d\n",
               fn.c_str(), tmpData, bytesRead);
        DumpHeader("Sequence File head:", tmpData, bytesRead);
        fclose(seqFile);
        return nullptr;
    }
    if (bytesRead < 8) {
        LogErr(VB_SEQUENCE, "Sequence file %s too short, unable to read FSEQ version fields\n", fn.c_str());
        DumpHeader("Sequence File head:", tmpData, bytesRead);
        fclose(seqFile);
        return nullptr;
    }
    int seqVersionMinor = tmpData[6];
    int seqVersionMajor = tmpData[7];

    ///////////////////////////////////////////////////////////////////////
    // Get Channel Data Offset
    uint64_t seqChanDataOffset = read2ByteUInt(&tmpData[4]);
    
    if (tmpData[0] == 'E') {
        //v1 eseq file.  This is basically an uncompressed v2 file with a custom header
        seqChanDataOffset = 20;
        seqVersionMajor = 2;
        seqVersionMinor = 0;
    }
    std::vector<uint8_t> header(seqChanDataOffset);
    fseeko(seqFile, 0L, SEEK_SET);
    bytesRead = fread(&header[0], 1, seqChanDataOffset, seqFile);
    if (bytesRead != seqChanDataOffset) {
        LogErr(VB_SEQUENCE, "Error opening sequence file: %s. Could not read header.\n", fn.c_str());
        DumpHeader("Sequence File head:", &header[0], bytesRead);
        fclose(seqFile);
        return nullptr;
    }

    FSEQFile *file = nullptr;
    if (seqVersionMajor == 1) {
        file = new V1FSEQFile(fn, seqFile, header);
    } else if (seqVersionMajor == 2) {
        file = new V2FSEQFile(fn, seqFile, header);
    } else {
        LogErr(VB_SEQUENCE, "Error opening sequence file: %s. Unknown FSEQ version %d-%d\n",
               fn.c_str(), seqVersionMajor, seqVersionMinor);
        DumpHeader("Sequence File head:", tmpData, bytesRead);
        fclose(seqFile);
        return nullptr;
    }
    file->dumpInfo();
    return file;
}
FSEQFile* FSEQFile::createFSEQFile(const std::string &fn,
                                   int version,
                                   CompressionType ct,
                                   int level) {
    if (version == 1) {
        return new V1FSEQFile(fn);
    }
    return new V2FSEQFile(fn, ct, level);
}
std::string FSEQFile::getMediaFilename(const std::string &fn) {
    std::unique_ptr<FSEQFile> file(FSEQFile::openFSEQFile(fn));
    if (file) {
        return file->getMediaFilename();
    }
    return "";
}
std::string FSEQFile::getMediaFilename() const {
    for (auto &a : m_variableHeaders) {
        if (a.code[0] == 'm' && a.code[1] == 'f') {
            const char *d = (const char *)&a.data[0];
            return d;
        }
    }
    return "";
}

FSEQFile::FSEQFile(const std::string &fn)
    : m_filename(fn),
    m_seqNumFrames(0),
    m_seqChannelCount(0),
    m_seqStepTime(50),
    m_variableHeaders(),
    m_uniqueId(0),
    m_seqFileSize(0),
    m_seqVersionMajor(1),
    m_seqVersionMinor(0),
    m_memoryBuffer(),
    m_seqChanDataOffset(0),
    m_memoryBufferPos(0)
{
    if (fn == "-memory-") {
        m_seqFile = nullptr;
        m_memoryBuffer.reserve(1024*1024);
    } else {
        m_seqFile = fopen((const char *)fn.c_str(), "wb");
    }
}

void FSEQFile::dumpInfo(bool indent) {
    char ind[5] = "    ";
    if (!indent) {
        ind[0] = 0;
    }
    LogDebug(VB_SEQUENCE, "%sSequence File Information\n", ind);
    LogDebug(VB_SEQUENCE, "%sseqFilename           : %s\n", ind, m_filename.c_str());
    LogDebug(VB_SEQUENCE, "%sseqVersion            : %d.%d\n", ind, m_seqVersionMajor, m_seqVersionMinor);
    LogDebug(VB_SEQUENCE, "%sseqChanDataOffset     : %d\n", ind, m_seqChanDataOffset);
    LogDebug(VB_SEQUENCE, "%sseqChannelCount       : %d\n", ind, m_seqChannelCount);
    LogDebug(VB_SEQUENCE, "%sseqNumPeriods         : %d\n", ind, m_seqNumFrames);
    LogDebug(VB_SEQUENCE, "%sseqStepTime           : %dms\n", ind, m_seqStepTime);
}


void FSEQFile::initializeFromFSEQ(const FSEQFile& fseq) {
    m_seqNumFrames = fseq.m_seqNumFrames;
    m_seqChannelCount = fseq.m_seqChannelCount;
    m_seqStepTime = fseq.m_seqStepTime;
    m_variableHeaders = fseq.m_variableHeaders;
    m_uniqueId = fseq.m_uniqueId;
}


FSEQFile::FSEQFile(const std::string &fn, FILE *file, const std::vector<uint8_t> &header)
    : m_filename(fn),
    m_seqFile(file),
    m_uniqueId(0),
    m_memoryBuffer(),
    m_memoryBufferPos(0)
{
    fseeko(m_seqFile, 0L, SEEK_END);
    m_seqFileSize = ftello(m_seqFile);
    fseeko(m_seqFile, 0L, SEEK_SET);

    if (header[0] == 'E') {
        m_seqChanDataOffset = 20;
        m_seqVersionMinor = 0;
        m_seqVersionMajor = 2;
        m_seqChannelCount = read4ByteUInt(&header[8]);
        m_seqStepTime = 50;
        m_seqNumFrames = (m_seqFileSize - 20) / m_seqChannelCount;
    } else {
        m_seqChanDataOffset = read2ByteUInt(&header[4]);
        m_seqVersionMinor = header[6];
        m_seqVersionMajor = header[7];
        m_seqChannelCount = read4ByteUInt(&header[10]);
        m_seqNumFrames = read4ByteUInt(&header[14]);
        m_seqStepTime = header[18];
    }
}
FSEQFile::~FSEQFile() {
    if (m_seqFile) {
        fclose(m_seqFile);
    }
}

int FSEQFile::seek(uint64_t location, int origin) {
    if (m_seqFile) {
        return fseeko(m_seqFile, location, origin);
    } else if (origin == SEEK_SET) {
        m_memoryBufferPos = location;
    } else if (origin == SEEK_CUR) {
        m_memoryBufferPos += location;
    } else if (origin == SEEK_END) {
        m_memoryBufferPos = m_memoryBuffer.size();
    }
    return 0;
}

uint64_t FSEQFile::tell() {
    if (m_seqFile) {
       return ftello(m_seqFile);
    }
    return m_memoryBufferPos;
}

uint64_t FSEQFile::write(const void * ptr, uint64_t size) {
    if (m_seqFile) {
        return fwrite(ptr, 1, size, m_seqFile);
    }
    if ((m_memoryBufferPos + size) > m_memoryBuffer.size()) {
        m_memoryBuffer.resize(m_memoryBufferPos + size);
    }
    memcpy(&m_memoryBuffer[m_memoryBufferPos], ptr, size);
    m_memoryBufferPos += size;
    return size;
}

uint64_t FSEQFile::read(void *ptr, uint64_t size) {
    return fread(ptr, 1, size, m_seqFile);
}

void FSEQFile::preload(uint64_t pos, uint64_t size) {
#ifndef PLATFORM_UNKNOWN
    if (posix_fadvise(fileno(m_seqFile), pos, size, POSIX_FADV_WILLNEED) != 0) {
        LogErr(VB_SEQUENCE, "Could not advise kernel %d  size: %d\n", (int)pos, (int)size);
    }
#endif
}

void FSEQFile::parseVariableHeaders(const std::vector<uint8_t> &header, int start) {
    while (start < header.size() - 5) {
        int len = read2ByteUInt(&header[start]);
        if (len) {
            VariableHeader vheader;
            vheader.code[0] = header[start + 2];
            vheader.code[1] = header[start + 3];
            vheader.data.resize(len - 4);
            memcpy(&vheader.data[0], &header[start + 4], len - 4);
            m_variableHeaders.push_back(vheader);
        } else {
            len += 4;
        }
        start += len;
    }
}
void FSEQFile::finalize() {
    fflush(m_seqFile);
}


V1FSEQFile::V1FSEQFile(const std::string &fn)
  : FSEQFile(fn), m_dataBlockSize(0)
{
}

void V1FSEQFile::writeHeader() {
    static int fixedHeaderLength = 28;
    uint8_t header[28];
    memset(header, 0, 28);
    header[0] = 'P';
    header[1] = 'S';
    header[2] = 'E';
    header[3] = 'Q';

    // data offset
    uint32_t dataOffset = fixedHeaderLength;
    for (auto &a : m_variableHeaders) {
        dataOffset += a.data.size() + 4;
    }
    dataOffset = roundTo4(dataOffset);
    write2ByteUInt(&header[4], dataOffset);

    header[6] = 0; //minor
    header[7] = 1; //major
    m_seqVersionMinor = header[6];
    m_seqVersionMajor = header[7];
    // Fixed header length
    write2ByteUInt(&header[8], fixedHeaderLength);
    // Step Size
    write4ByteUInt(&header[10], m_seqChannelCount);
    // Number of Steps
    write4ByteUInt(&header[14], m_seqNumFrames);
    // Step time in ms
    header[18] = m_seqStepTime;
    //flags
    header[19] = 0;
    // universe count
    write2ByteUInt(&header[20], 0);
    // universe Size
    write2ByteUInt(&header[22], 0);
    // universe Size
    header[24] = 1;
    // color order
    header[25] = 2;
    header[26] = 0;
    header[27] = 0;
    write(header, 28);
    for (auto &a : m_variableHeaders) {
        uint8_t buf[4];
        uint32_t len = a.data.size() + 4;
        write2ByteUInt(buf, len);
        buf[2] = a.code[0];
        buf[3] = a.code[1];
        write(buf, 4);
        write(&a.data[0], a.data.size());
    }
    uint64_t pos = tell();
#ifdef _MSC_VER
    wxASSERT(pos <= dataOffset); // i dont see how this could be wrong but seeing some crashes
#endif
    if (pos != dataOffset) {
        char buf[4] = {0,0,0,0};
#ifdef _MSC_VER
        wxASSERT(dataOffset - pos <= 4); // i dont see how this could be wrong but seeing some crashes
#endif
        write(buf, dataOffset - pos);
    }
    m_seqChanDataOffset = dataOffset;
    dumpInfo(false);
}

V1FSEQFile::V1FSEQFile(const std::string &fn, FILE *file, const std::vector<uint8_t> &header)
: FSEQFile(fn, file, header) {

    // m_seqNumUniverses = (header[20])       + (header[21] << 8);
    // m_seqUniverseSize = (header[22])       + (header[23] << 8);
    // m_seqGamma         = header[24];
    // m_seqColorEncoding = header[25];

    // 0 = header[26]
    // 0 = header[27]
    parseVariableHeaders(header, 28);

    //use the last modified time for the uniqueId
    struct stat stats;
    fstat(fileno(file), &stats);
    m_uniqueId = stats.st_mtime;
}

V1FSEQFile::~V1FSEQFile() {

}

class UncompressedFrameData : public FSEQFile::FrameData {
public:
    UncompressedFrameData(uint32_t frame,
                          uint32_t sz,
                          const std::vector<std::pair<uint32_t, uint32_t>> &ranges)
    : FrameData(frame), m_ranges(ranges) {
        m_size = sz;
        m_data = (uint8_t*)malloc(sz);
    }
    virtual ~UncompressedFrameData() {
        if (m_data != nullptr) {
            free(m_data);
        }
    }

    virtual bool readFrame(uint8_t *data, uint32_t maxChannels) override {
        if (m_data == nullptr) return false;
        uint32_t offset = 0;
        for (auto &rng : m_ranges) {
            uint32_t toRead = rng.second;
            if (offset + toRead <= m_size) {
                uint32_t toCopy = std::min(toRead, maxChannels - rng.first);
                memcpy(&data[rng.first], &m_data[offset], toCopy);
                offset += toRead;
            } else {
                return false;
            }
        }
        return true;
    }

    uint32_t m_size;
    uint8_t *m_data;
    std::vector<std::pair<uint32_t, uint32_t>> m_ranges;
};

void V1FSEQFile::prepareRead(const std::vector<std::pair<uint32_t, uint32_t>> &ranges, uint32_t startFrame) {
    m_rangesToRead = ranges;
    m_dataBlockSize = 0;
    for (auto &rng : m_rangesToRead) {
        //make sure we don't read beyond the end of the sequence data
        int toRead = rng.second;
        if ((rng.first + toRead) > m_seqChannelCount) {
            toRead = m_seqChannelCount - rng.first;
            rng.second = toRead;
        }
        m_dataBlockSize += toRead;
    }
    FrameData *f = getFrame(startFrame);
    if (f) {
        delete f;
    }
}

FrameData *V1FSEQFile::getFrame(uint32_t frame) {
    if (m_rangesToRead.empty()) {
        std::vector<std::pair<uint32_t, uint32_t>> range;
        range.push_back(std::pair<uint32_t, uint32_t>(0, m_seqChannelCount));
        prepareRead(range, frame);
    }
    uint64_t offset = m_seqChannelCount;
    offset *= frame;
    offset += m_seqChanDataOffset;

    UncompressedFrameData *data = new UncompressedFrameData(frame, m_dataBlockSize, m_rangesToRead);
    if (seek(offset, SEEK_SET)) {
        LogErr(VB_SEQUENCE, "Failed to seek to proper offset for channel data for frame %d! %" PRIu64 "\n", frame, offset);
        return data;
    }
    uint32_t sz = 0;
    //read the ranges into the buffer
    for (auto &rng : data->m_ranges) {
        if (rng.first < m_seqChannelCount) {
            int toRead = rng.second;
            uint64_t doffset = offset;
            doffset += rng.first;
            seek(doffset, SEEK_SET);
            size_t bread = read(&data->m_data[sz], toRead);
            if (bread != toRead) {
                LogErr(VB_SEQUENCE, "Failed to read channel data for frame %d!   Needed to read %d but read %d\n",
                       frame, toRead, (int)bread);
            }
            sz += toRead;
        }
    }
    return data;
}

void V1FSEQFile::addFrame(uint32_t frame,
                          const uint8_t *data) {
    write(data, m_seqChannelCount);
}

void V1FSEQFile::finalize() {
    FSEQFile::finalize();
}

uint32_t V1FSEQFile::getMaxChannel() const {
    return m_seqChannelCount;
}

static const int V2FSEQ_HEADER_SIZE = 32;
static const int V2FSEQ_MINOR_VERSION = 0;
static const int V2FSEQ_MAJOR_VERSION = 2;
static const int V2FSEQ_VARIABLE_HEADER_SIZE = 4;
static const int V2FSEQ_SPARSE_RANGE_SIZE = 6;
static const int V2FSEQ_COMPRESSION_BLOCK_SIZE = 8;
#if !defined(NO_ZLIB) || !defined(NO_ZSTD)
static const int V2FSEQ_OUT_BUFFER_SIZE = 1024*1024; //1M output buffer
static const int V2FSEQ_OUT_BUFFER_FLUSH_SIZE = 900 * 1024; //90% full, flush it
#endif

class V2Handler {
public:
    V2Handler(V2FSEQFile *f)
        : m_file(f)
    {
        m_seqChanDataOffset = f->m_seqChanDataOffset;
    }
    virtual ~V2Handler() {}


    virtual uint8_t getCompressionType() = 0;
    virtual FrameData *getFrame(uint32_t frame) = 0;

    virtual uint32_t computeMaxBlocks() = 0;
    virtual void addFrame(uint32_t frame, const uint8_t *data) = 0;
    virtual void finalize() = 0;
    virtual std::string GetType() const = 0;

    int seek(uint64_t location, int origin) {
        return m_file->seek(location, origin);
    }
    uint64_t tell() {
        return m_file->tell();
    }
    uint64_t write(const void * ptr, uint64_t size) {
        return m_file->write(ptr, size);
    }
    uint64_t read(void *ptr, uint64_t size) {
        return m_file->read(ptr, size);
    }
    void preload(uint64_t pos, uint64_t size) {
        m_file->preload(pos, size);
    }

    virtual void prepareRead(uint32_t frame) {}
    
    V2FSEQFile *m_file;
    uint64_t   m_seqChanDataOffset;
};

class V2NoneCompressionHandler : public V2Handler {
public:
    V2NoneCompressionHandler(V2FSEQFile *f) : V2Handler(f) {}
    virtual ~V2NoneCompressionHandler() {}

    virtual uint8_t getCompressionType() override { return 0;}
    virtual uint32_t computeMaxBlocks() override {return 0;}
    virtual std::string GetType() const override { return "No Compression"; }
    virtual void prepareRead(uint32_t frame) {
        FrameData *f = getFrame(frame);
        if (f) {
            delete f;
        }
    }
    virtual FrameData *getFrame(uint32_t frame) override {
        UncompressedFrameData *data = new UncompressedFrameData(frame, m_file->m_dataBlockSize, m_file->m_rangesToRead);
        uint64_t offset = m_file->getChannelCount();
        offset *= frame;
        offset += m_seqChanDataOffset;
        if (seek(offset, SEEK_SET)) {
            LogErr(VB_SEQUENCE, "Failed to seek to proper offset for channel data! %" PRIu64 "\n", offset);
            return data;
        }
        if (m_file->m_sparseRanges.empty()) {
            uint32_t sz = 0;
            //read the ranges into the buffer
            for (auto &rng : data->m_ranges) {
                if (rng.first < m_file->getChannelCount()) {
                    int toRead = rng.second;
                    uint64_t doffset = offset;
                    doffset += rng.first;
                    seek(doffset, SEEK_SET);
                    size_t bread = read(&data->m_data[sz], toRead);
                    if (bread != toRead) {
                        LogErr(VB_SEQUENCE, "Failed to read channel data!   Needed to read %d but read %d\n", toRead, (int)bread);
                    }
                    sz += toRead;
                }
            }
        } else {
            size_t bread = read(data->m_data, m_file->m_dataBlockSize);
            if (bread != m_file->m_dataBlockSize) {
                LogErr(VB_SEQUENCE, "Failed to read channel data!   Needed to read %d but read %d\n", m_file->m_dataBlockSize, (int)bread);
            }
        }
        return data;
    }
    virtual void addFrame(uint32_t frame, const uint8_t *data) override {
        if (m_file->m_sparseRanges.empty()) {
            write(data, m_file->getChannelCount());
        } else {
            for (auto &a : m_file->m_sparseRanges) {
                write(&data[a.first], a.second);
            }
        }
    }

    virtual void finalize() override {}

};
class V2CompressedHandler : public V2Handler {
public:
    V2CompressedHandler(V2FSEQFile *f) : V2Handler(f), m_maxBlocks(0), m_curBlock(99999), m_framesPerBlock(0), m_curFrameInBlock(0), m_readThread(nullptr) {
        if (!m_file->m_frameOffsets.empty()) {
            m_maxBlocks = m_file->m_frameOffsets.size() - 1;
        }
    }
    virtual ~V2CompressedHandler() {
        if (m_readThread) {
            m_readThreadRunning = false;
            m_readSignal.notify_all();
            m_readThread->join();
        }
        for (auto &a : m_blockMap) {
            if (a.second) {
                free(a.second);
            }
        }
        m_blockMap.clear();
    }

    virtual uint32_t computeMaxBlocks() override {
        if (m_maxBlocks > 0) {
            return m_maxBlocks;
        }
        //determine a good number of compression blocks
        uint64_t datasize = m_file->getChannelCount() * m_file->getNumFrames();
        uint64_t numBlocks = datasize;
        numBlocks /= (64*1024); //at least 64K per block
        if (numBlocks > 255) {
            //need a lot of blocks, use as many as we can
            numBlocks = 255;
        } else if (numBlocks < 1) {
            numBlocks = 1;
        }
        m_framesPerBlock = m_file->getNumFrames() / numBlocks;
        if (m_framesPerBlock < 10) m_framesPerBlock = 10;
        m_curFrameInBlock = 0;
        m_curBlock = 0;

        numBlocks = m_file->getNumFrames() / m_framesPerBlock + 1;
        while (numBlocks > 255) {
            m_framesPerBlock++;
            numBlocks = m_file->getNumFrames() / m_framesPerBlock + 1;
        }
        // first block is going to be smaller, so add some blocks
        if (numBlocks < 254) {
            numBlocks += 2;
        } else if (numBlocks < 255) {
            numBlocks++;
        }
        m_maxBlocks = numBlocks;
        m_curBlock = 0;
        return m_maxBlocks;
    }

    virtual void finalize() override {
        uint64_t curr = tell();
        uint64_t off = V2FSEQ_HEADER_SIZE;
        seek(off, SEEK_SET);
        int count = m_file->m_frameOffsets.size();
        if (count == 0) {
            //not good, count should never be 0 unless no frames were written,
            //this really should never happen, but I've seen fseq files without the
            //blocks filled in so I know it DOES happen, just haven't figured out
            //how it's possible yet.
            LogErr(VB_SEQUENCE, "Error writing fseq file.  No compressed blocks created.\n");
            
            //we'll use the offset to the data for the 0 frame
            seek(0, SEEK_SET);
            uint8_t header[10];
            read(header, 10);
            int seqChanDataOffset = read2ByteUInt(&header[4]);
            seek(off, SEEK_SET);
            m_file->m_frameOffsets.push_back(std::pair<uint32_t, uint64_t>(0, seqChanDataOffset));
            count++;
        }
        m_file->m_frameOffsets.push_back(std::pair<uint32_t, uint64_t>(99999999, curr));
        for (int x = 0 ; x < count; x++) {
            uint8_t buf[8];
            uint32_t frame = m_file->m_frameOffsets[x].first;
            write4ByteUInt(buf,frame);

            uint64_t len64 = m_file->m_frameOffsets[x + 1].second;
            len64 -= m_file->m_frameOffsets[x].second;
            uint32_t len = len64;
            write4ByteUInt(&buf[4], len);
            write(buf, 8);
            //printf("%d    %d: %d\n", x, frame, len);
        }
        m_file->m_frameOffsets.pop_back();
        seek(curr, SEEK_SET);
    }

    virtual void prepareRead(uint32_t frame) override {
        //start reading the first couple blocks immediately
        int block = 0;
        while (frame >= m_file->m_frameOffsets[block + 1].first) {
            block++;
        }
        
        LogDebug(VB_SEQUENCE, "Preparing to read starting frame:  %d    block: %d\n", frame, block);
        m_blocksToRead.push_back(block);
        m_blocksToRead.push_back(block + 1);
        m_blocksToRead.push_back(block + 2);
        m_blocksToRead.push_back(block + 3);
        m_firstBlock = block;
        m_readThreadRunning = true;
        m_readThread = new std::thread([this]() {
            while (m_readThreadRunning) {
                std::unique_lock<std::mutex> readerlock(m_readMutex);
                if (!m_blocksToRead.empty()) {
                    int block = m_blocksToRead.front();
                    m_blocksToRead.pop_front();
                    uint8_t *data = m_blockMap[block];
                    if (!data && block < (m_file->m_frameOffsets.size() - 1)) {
                        readerlock.unlock();
                        uint64_t offset = m_file->m_frameOffsets[block].second;
                        uint64_t size = m_file->m_frameOffsets[block + 1].second - offset;
                        int max = m_file->getNumFrames() * m_file->getChannelCount();
                        bool problem = false;
                        if (size > max) {
                            size = max;
                            problem = true;
                        }
                        data = (uint8_t*)malloc(size);
                        if (!data || problem) {
                            //this is a serious problem, I need to figure out why this is occuring
                            LogWarn(VB_SEQUENCE, "Serious problem reading sequence data\n");
                            LogWarn(VB_SEQUENCE, "    Block: %d / %d\n", block, m_file->m_frameOffsets.size());
                            LogWarn(VB_SEQUENCE, "    Offset: %d\n", m_file->m_frameOffsets[block].second);
                            LogWarn(VB_SEQUENCE, "    Offset+1: %d\n", m_file->m_frameOffsets[block + 1].second);
                            int sz = m_file->m_frameOffsets[block + 1].second - offset;
                            LogWarn(VB_SEQUENCE, "    Size: %d\n", (int)sz);
                            LogWarn(VB_SEQUENCE, "    Max: %d\n", (int)max);
                            for (int x = 0; x < m_file->m_frameOffsets.size(); x++) {
                                LogWarn(VB_SEQUENCE, "        Block %d:    Offset: %d    Size: %d\n", x, m_file->m_frameOffsets[block].first,
                                        m_file->m_frameOffsets[block].second);
                            }
                        }
                        seek(offset, SEEK_SET);
                        read(data, size);

                        readerlock.lock();
                        m_blockMap[block] = data;
                        m_readSignal.notify_all();
                    }
                } else {
                    m_readSignal.wait_for(readerlock, 25ms);
                }
            }
        });
    }
    
    void preloadBlock(int block) {
        for (int b = block; b < block + 4; b++) {
            //let the kernel know that we'll likely need the next few blocks in the near future
            if (b < m_file->m_frameOffsets.size() - 1) {
                uint64_t len2 = m_file->m_frameOffsets[b + 1].second;
                if (b < m_file->m_frameOffsets.size() - 2) {
                    len2 = m_file->m_frameOffsets[b + 2].second;
                }
                len2 -= m_file->m_frameOffsets[b].second;
                uint64_t pos = m_file->m_frameOffsets[b].second;
                preload(pos, len2);
            }
            std::unique_lock<std::mutex> readerlock(m_readMutex);
            m_blocksToRead.push_back(b);
            m_readSignal.notify_all();
        }
    }
    uint8_t *getBlock(int block) {
        std::unique_lock<std::mutex> readerlock(m_readMutex);
        uint8_t *data = m_blockMap[block];
        while (data == nullptr) {
            if (block > (m_firstBlock + 3)) {
                //if not one of the first few blocks and it's not already
                //available, then something is really slow
                AddSlowStorageWarning();
                LogWarn(VB_SEQUENCE, "Data block not available when needed %d/%d.  First block requested: %d.   Likely slow storage.\n", block, m_maxBlocks, m_firstBlock);
                LogWarn(VB_SEQUENCE, "Blocks: %d     First: %d\n", m_blocksToRead.size(), m_blocksToRead.empty() ? -1 : m_blocksToRead.front());
            }
            m_blocksToRead.push_front(block);
            m_readSignal.wait_for(readerlock, 10s);
            data = m_blockMap[block];
        }
        if (block > 2) {
            //clean up old blocks we don't need anymore
            uint8_t *old = m_blockMap[block - 2];
            m_blockMap[block - 2] = nullptr;
            free(old);
        }
        return data;
    }

    // for compressed files, this is the compression data
    uint32_t m_framesPerBlock;
    uint32_t m_curFrameInBlock;
    uint32_t m_curBlock;
    uint32_t m_maxBlocks;
    
    std::atomic_bool m_readThreadRunning;
    std::thread *m_readThread;
    std::mutex m_readMutex;
    std::map<int, uint8_t*> m_blockMap;
    std::list<int> m_blocksToRead;
    std::condition_variable m_readSignal;
    int m_firstBlock = 0;
};

#ifndef NO_ZSTD
class V2ZSTDCompressionHandler : public V2CompressedHandler {
public:
    V2ZSTDCompressionHandler(V2FSEQFile *f) : V2CompressedHandler(f),
    m_cctx(nullptr),
    m_dctx(nullptr)
    {
        m_outBuffer.pos = 0;
        m_outBuffer.size = V2FSEQ_OUT_BUFFER_SIZE;
        m_outBuffer.dst = malloc(m_outBuffer.size);
        m_inBuffer.src = nullptr;
        m_inBuffer.size = 0;
        m_inBuffer.pos = 0;
        LogDebug(VB_SEQUENCE, "  Prepared to read/write a ZSTD compress fseq file.\n");
    }
    virtual ~V2ZSTDCompressionHandler() {
        free(m_outBuffer.dst);
        if (m_cctx) {
            ZSTD_freeCStream(m_cctx);
        }
        if (m_dctx) {
            ZSTD_freeDStream(m_dctx);
        }
    }
    virtual uint8_t getCompressionType() override { return 1;}
    virtual std::string GetType() const override { return "Compressed ZSTD"; }

    virtual FrameData *getFrame(uint32_t frame) override {
        if (m_curBlock > 256 || (frame < m_file->m_frameOffsets[m_curBlock].first) || (frame >= m_file->m_frameOffsets[m_curBlock + 1].first)) {
            //frame is not in the current block
            m_curBlock = 0;
            while (frame >= m_file->m_frameOffsets[m_curBlock + 1].first) {
                m_curBlock++;
            }
            if (m_dctx == nullptr) {
                m_dctx = ZSTD_createDStream();
            }
            ZSTD_initDStream(m_dctx);
            
            uint64_t len = m_file->m_frameOffsets[m_curBlock + 1].second;
            len -= m_file->m_frameOffsets[m_curBlock].second;
            int max = m_file->getNumFrames() * m_file->getChannelCount();
            if (len > max) {
                len = max;
            }
            m_inBuffer.pos = 0;
            m_inBuffer.size = len;

            m_inBuffer.src = getBlock(m_curBlock);

            if (m_curBlock < m_file->m_frameOffsets.size() - 2) {
                //let the kernel know that we'll likely need the next block in the near future
                preloadBlock(m_curBlock + 1);
            }

            free(m_outBuffer.dst);
            m_framesPerBlock = (m_file->m_frameOffsets[m_curBlock + 1].first > m_file->getNumFrames() ? m_file->getNumFrames() :  m_file->m_frameOffsets[m_curBlock + 1].first) - m_file->m_frameOffsets[m_curBlock].first;
            m_outBuffer.size = m_framesPerBlock * m_file->getChannelCount();
            m_outBuffer.dst = malloc(m_outBuffer.size);
            m_outBuffer.pos = 0;
            m_curFrameInBlock = 0;
        }
        int fidx = frame - m_file->m_frameOffsets[m_curBlock].first;

        if (fidx >= m_curFrameInBlock) {
            m_outBuffer.size = (fidx + 1) * m_file->getChannelCount();
            ZSTD_decompressStream(m_dctx, &m_outBuffer, &m_inBuffer);
            m_curFrameInBlock = fidx + 1;
        }
        
        fidx *= m_file->getChannelCount();
        uint8_t *fdata = (uint8_t*)m_outBuffer.dst;
        UncompressedFrameData *data = new UncompressedFrameData(frame, m_file->m_dataBlockSize, m_file->m_rangesToRead);

        // This stops the crash on load ... but it is not the root cause.
        // But better to not load completely than crashing
        if (fidx < 0) {
            // this is not going to end well ... best to give up here
            LogErr(VB_SEQUENCE, "Frame index calculated as a negative number. Aborting frame %d load.\n", (int)frame);
            return data;
        }

        if (!m_file->m_sparseRanges.empty()) {
            memcpy(data->m_data, &fdata[fidx], m_file->getChannelCount());
        } else {
            uint32_t sz = 0;
            //read the ranges into the buffer
            for (auto &rng : data->m_ranges) {
                if (rng.first < m_file->getChannelCount()) {
                    int start = fidx + rng.first;
                    memcpy(&data->m_data[sz], &fdata[start], rng.second);
                    sz += rng.second;
                }
            }
        }

        return data;
    }
    void compressData(ZSTD_CStream* m_cctx, ZSTD_inBuffer_s &input, ZSTD_outBuffer_s &output) {
        ZSTD_compressStream(m_cctx, &output, &input);
        int count = input.pos;
        int total = input.size;
        uint8_t *curData = (uint8_t*)input.src;
        while (count < total) {
            count += input.pos;
            curData += input.pos;
            input.src = curData;
            input.size -= input.pos;
            input.pos = 0;
            if (output.pos) {
                write(output.dst, output.pos);
                output.pos = 0;
            }
            ZSTD_compressStream(m_cctx, &output, &input);
            count += input.pos;
        }
    }
    virtual void addFrame(uint32_t frame, const uint8_t *data) override {

        if (m_cctx == nullptr) {
            m_cctx = ZSTD_createCStream();
        }
        if (m_curFrameInBlock == 0) {
            uint64_t offset = tell();
            //LogDebug(VB_SEQUENCE, "  Preparing to create a compressed block of data starting at frame %d, offset  %" PRIu64 ".\n", frame, offset);
            m_file->m_frameOffsets.push_back(std::pair<uint32_t, uint64_t>(frame, offset));
            int clevel = m_file->m_compressionLevel == -99 ? 1 : m_file->m_compressionLevel;
            if (clevel < -25 || clevel > 25) {
                clevel = 1;
            }
            if (frame == 0 && (ZSTD_versionNumber() > 10305)) {
                // first frame needs to be grabbed as fast as possible
                // or remotes may be off by a few frames at start.  Thus,
                // if using recent zstd, we'll use the negative levels
                // for the first block so the decompression can
                // be as fast as possible
                clevel = -10;
            }
            if (ZSTD_versionNumber() <= 10305 && clevel < 0) {
                clevel = 0;
            }
            ZSTD_initCStream(m_cctx, clevel);
        }

        uint8_t *curData = (uint8_t *)data;
        if (m_file->m_sparseRanges.empty()) {
            ZSTD_inBuffer_s input = {
                curData,
                m_file->getChannelCount(),
                0
            };
            compressData(m_cctx, input, m_outBuffer);
        } else {
            for (auto &a : m_file->m_sparseRanges) {
                ZSTD_inBuffer_s input = {
                    &curData[a.first],
                    a.second,
                    0
                };
                compressData(m_cctx, input, m_outBuffer);
            }
        }

        if (m_outBuffer.pos > V2FSEQ_OUT_BUFFER_FLUSH_SIZE) {
            //buffer is getting full, better flush it
            write(m_outBuffer.dst, m_outBuffer.pos);
            m_outBuffer.pos = 0;
        }

        m_curFrameInBlock++;
        //if we hit the max per block OR we're in the first block and hit frame #10
        //we'll start a new block.  We want the first block to be small so startup is
        //quicker and we can get the first few frames as fast as possible.
        if ((m_curBlock == 0 && m_curFrameInBlock == 10)
            || (m_curFrameInBlock >= m_framesPerBlock && m_file->m_frameOffsets.size() < m_maxBlocks)) {
            while(ZSTD_endStream(m_cctx, &m_outBuffer) > 0) {
                write(m_outBuffer.dst, m_outBuffer.pos);
                m_outBuffer.pos = 0;
            }
            write(m_outBuffer.dst, m_outBuffer.pos);
            //LogDebug(VB_SEQUENCE, "  Finalized block of data ending at frame %d.  Frames in block: %d.\n", frame, m_curFrameInBlock);
            m_outBuffer.pos = 0;
            m_curFrameInBlock = 0;
            m_curBlock++;
        }
    }
    virtual void finalize() override {
        if (m_curFrameInBlock) {
            while(ZSTD_endStream(m_cctx, &m_outBuffer) > 0) {
                write(m_outBuffer.dst, m_outBuffer.pos);
                m_outBuffer.pos = 0;
            }
            write(m_outBuffer.dst, m_outBuffer.pos);
            LogDebug(VB_SEQUENCE, "  Finalized last block of data.  Frames in block: %d.\n", m_curFrameInBlock);
            m_outBuffer.pos = 0;
            m_curFrameInBlock = 0;
            m_curBlock++;
        }
        V2CompressedHandler::finalize();
    }

    ZSTD_CStream* m_cctx;
    ZSTD_DStream* m_dctx;
    ZSTD_outBuffer_s m_outBuffer;
    ZSTD_inBuffer_s m_inBuffer;
};
#endif

#ifndef NO_ZLIB
class V2ZLIBCompressionHandler : public V2CompressedHandler {
public:
    V2ZLIBCompressionHandler(V2FSEQFile *f) : V2CompressedHandler(f), m_stream(nullptr), m_outBuffer(nullptr), m_inBuffer(nullptr) {
    }
    virtual ~V2ZLIBCompressionHandler() {
        if (m_outBuffer) {
            free(m_outBuffer);
        }
    }
    virtual uint8_t getCompressionType() override { return 2; }
    virtual std::string GetType() const override { return "Compressed ZLIB"; }

    virtual FrameData *getFrame(uint32_t frame) override {
        if (m_curBlock > 256 || (frame < m_file->m_frameOffsets[m_curBlock].first) || (frame >= m_file->m_frameOffsets[m_curBlock + 1].first)) {
            //frame is not in the current block
            m_curBlock = 0;
            while (frame >= m_file->m_frameOffsets[m_curBlock + 1].first) {
                m_curBlock++;
            }
            
            uint64_t len = m_file->m_frameOffsets[m_curBlock + 1].second;
            len -= m_file->m_frameOffsets[m_curBlock].second;
            m_inBuffer = getBlock(m_curBlock);

            if (m_curBlock < m_file->m_frameOffsets.size() - 2) {
                //let the kernel know that we'll likely need the next block in the near future
                preloadBlock(m_curBlock + 1);
            }

            if (m_stream == nullptr) {
                m_stream = (z_stream*)calloc(1, sizeof(z_stream));
                m_stream->next_in = m_inBuffer;
                m_stream->avail_in = len;
                inflateInit(m_stream);
            }
            if (m_outBuffer != nullptr) {
                free(m_outBuffer);
            }
            int numFrames = (m_file->m_frameOffsets[m_curBlock + 1].first > m_file->getNumFrames() ? m_file->getNumFrames() :  m_file->m_frameOffsets[m_curBlock + 1].first) - m_file->m_frameOffsets[m_curBlock].first;
            int outsize = numFrames * m_file->getChannelCount();
            m_outBuffer = (uint8_t*)malloc(outsize);
            m_stream->next_out = m_outBuffer;
            m_stream->avail_out = outsize;

            inflate(m_stream, Z_SYNC_FLUSH);
            inflateEnd(m_stream);
            free(m_stream);
            m_stream = nullptr;
        }
        int fidx = frame - m_file->m_frameOffsets[m_curBlock].first;
        fidx *= m_file->getChannelCount();
        uint8_t *fdata = (uint8_t*)m_outBuffer;
        UncompressedFrameData *data = new UncompressedFrameData(frame, m_file->m_dataBlockSize, m_file->m_rangesToRead);
        if (!m_file->m_sparseRanges.empty()) {
            memcpy(data->m_data, &fdata[fidx], m_file->getChannelCount());
        } else {
            uint32_t sz = 0;
            //read the ranges into the buffer
            for (auto &rng : data->m_ranges) {
                if (rng.first < m_file->getChannelCount()) {
                    memcpy(&data->m_data[sz], &fdata[fidx + rng.first], rng.second);
                    sz += rng.second;
                }
            }
        }
        return data;
    }
    virtual void addFrame(uint32_t frame, const uint8_t *data) override {
        if (m_outBuffer == nullptr) {
            m_outBuffer = (uint8_t*)malloc(V2FSEQ_OUT_BUFFER_SIZE);
        }
        if (m_stream == nullptr) {
            m_stream = (z_stream*)calloc(1, sizeof(z_stream));
        }
        if (m_curFrameInBlock == 0) {
            uint64_t offset = tell();
            m_file->m_frameOffsets.push_back(std::pair<uint32_t, uint64_t>(frame, offset));
            deflateEnd(m_stream);
            memset(m_stream, 0, sizeof(z_stream));
        }
        if (m_curFrameInBlock == 0) {
            int clevel = m_file->m_compressionLevel == -99 ? 1 : m_file->m_compressionLevel;
            if (clevel < 0 || clevel > 9) {
                clevel = 1;
            }
            deflateInit(m_stream, clevel);
            m_stream->next_out = m_outBuffer;
            m_stream->avail_out = V2FSEQ_OUT_BUFFER_SIZE;
        }

        uint8_t *curData = (uint8_t *)data;
        if (m_file->m_sparseRanges.empty()) {
            m_stream->next_in = curData;
            m_stream->avail_in = m_file->getChannelCount();
            deflate(m_stream, 0);
        } else {
            for (auto &a : m_file->m_sparseRanges) {
                m_stream->next_in = &curData[a.first];
                m_stream->avail_in = a.second;
                deflate(m_stream, 0);
            }
        }
        if (m_stream->avail_out < (V2FSEQ_OUT_BUFFER_SIZE - V2FSEQ_OUT_BUFFER_FLUSH_SIZE)) {
            //buffer is getting full, better flush it
            uint64_t sz = V2FSEQ_OUT_BUFFER_SIZE;
            sz -= m_stream->avail_out;
            write(m_outBuffer, sz);
            m_stream->next_out = m_outBuffer;
            m_stream->avail_out = V2FSEQ_OUT_BUFFER_SIZE;
        }
        m_curFrameInBlock++;
        //if we hit the max per block OR we're in the first block and hit frame #10
        //we'll start a new block.  We want the first block to be small so startup is
        //quicker and we can get the first few frames as fast as possible.
        if ((m_curBlock == 0 && m_curFrameInBlock == 10)
            || (m_curFrameInBlock == m_framesPerBlock && m_file->m_frameOffsets.size() < m_maxBlocks)) {

            while (deflate(m_stream, Z_FINISH) != Z_STREAM_END) {
                uint64_t sz = V2FSEQ_OUT_BUFFER_SIZE;
                sz -= m_stream->avail_out;
                write(m_outBuffer, sz);
                m_stream->next_out = m_outBuffer;
                m_stream->avail_out = V2FSEQ_OUT_BUFFER_SIZE;
            }
            uint64_t sz = V2FSEQ_OUT_BUFFER_SIZE;
            sz -= m_stream->avail_out;
            write(m_outBuffer, sz);
            m_stream->next_out = m_outBuffer;
            m_stream->avail_out = V2FSEQ_OUT_BUFFER_SIZE;

            m_curFrameInBlock = 0;
            m_curBlock++;
        }
    }
    virtual void finalize() override {
        if (m_curFrameInBlock) {
            while (deflate(m_stream, Z_FINISH) != Z_STREAM_END) {
                uint64_t sz = V2FSEQ_OUT_BUFFER_SIZE;
                sz -= m_stream->avail_out;
                write(m_outBuffer, sz);
                m_stream->next_out = m_outBuffer;
                m_stream->avail_out = V2FSEQ_OUT_BUFFER_SIZE;
            }
            uint64_t sz = V2FSEQ_OUT_BUFFER_SIZE;
            sz -= m_stream->avail_out;
            write(m_outBuffer, sz);
            m_stream->next_out = m_outBuffer;
            m_stream->avail_out = V2FSEQ_OUT_BUFFER_SIZE;

            m_curFrameInBlock = 0;
            m_curBlock++;
        }
        V2CompressedHandler::finalize();
    }

    z_stream *m_stream;
    uint8_t *m_outBuffer;
    uint8_t *m_inBuffer;
};
#endif


void V2FSEQFile::createHandler() {
    switch (m_compressionType) {
    case CompressionType::none:
        m_handler = new V2NoneCompressionHandler(this);
        break;
    case CompressionType::zstd:
#ifdef NO_ZSTD
        LogErr(VB_ALL, "No support for zstd compression");
#else
        m_handler = new V2ZSTDCompressionHandler(this);
#endif
        break;
    case CompressionType::zlib:
#ifdef NO_ZLIB
        LogErr(VB_ALL, "No support for zlib compression");
#else
        m_handler = new V2ZLIBCompressionHandler(this);
#endif
        break;
    }
    if (m_handler == nullptr) {
        LogDebug(VB_SEQUENCE, "Creating a default none compression handler. %d", (int)m_compressionType);
        m_handler = new V2NoneCompressionHandler(this);
    }
}

V2FSEQFile::V2FSEQFile(const std::string &fn, CompressionType ct, int cl)
    : FSEQFile(fn),
    m_compressionType(ct),
    m_compressionLevel(cl),
    m_handler(nullptr)
{
    m_seqVersionMajor = V2FSEQ_MAJOR_VERSION;
    m_seqVersionMinor = V2FSEQ_MINOR_VERSION;
    createHandler();
}
void V2FSEQFile::writeHeader() {
    if (!m_sparseRanges.empty()) {
        //make sure the sparse ranges fit, and then
        //recalculate the channel count for in the fseq
        std::vector<std::pair<uint32_t, uint32_t>> newRanges;
        for (auto &a : m_sparseRanges) {
            if (a.first < m_seqChannelCount) {
                if (a.first + a.second > m_seqChannelCount) {
                    a.second = m_seqChannelCount - a.first;
                }
                newRanges.push_back(a);
            }
        }
        m_sparseRanges = newRanges;
        if (!m_sparseRanges.empty()) {
            m_seqChannelCount = 0;
            for (auto &a : m_sparseRanges) {
                m_seqChannelCount += a.second;
            }
        }
    }

	uint8_t maxBlocks = m_handler->computeMaxBlocks() & 0xFF;

	// Compute headerSize to include all data except channel data
	// This allows the use of a single buffer, and single write to disk
	int headerSize = V2FSEQ_HEADER_SIZE;
	headerSize += maxBlocks * V2FSEQ_COMPRESSION_BLOCK_SIZE;
	headerSize += m_sparseRanges.size() * V2FSEQ_SPARSE_RANGE_SIZE;
	for (auto &a : m_variableHeaders) {
		headerSize += V2FSEQ_VARIABLE_HEADER_SIZE;
		headerSize += a.data.size();
	}

	// Round to value of 4 for better memory alignment
	headerSize = roundTo4(headerSize);

    uint8_t header[headerSize];
    memset(header, 0, headerSize);

	// File identifier (PSEQ) - 4bytes
    header[0] = 'P';
    header[1] = 'S';
    header[2] = 'E';
    header[3] = 'Q';

	// Channel data start offset - 2 bytes
	m_seqChanDataOffset = headerSize;
    write2ByteUInt(&header[4], m_seqChanDataOffset);

	// File format version - 2 bytes
	header[6] = m_seqVersionMinor;
	header[7] = m_seqVersionMajor;

	// Computed header length - 2 bytes
	write2ByteUInt(&header[8], headerSize);
    // Channel count - 4 bytes
    write4ByteUInt(&header[10], m_seqChannelCount);
    // Number of frames - 4 bytes
    write4ByteUInt(&header[14], m_seqNumFrames);

    // Step time in milliseconds - 1 byte
    header[18] = m_seqStepTime;
    // Flags (unused & reserved, should be 0) - 1 byte
    header[19] = 0;
    // Compression type - 1 byte
    header[20] = m_handler->getCompressionType();
    // Number of blocks in compressed channel data (should be 0 if not compressed) - 1 byte
    header[21] = maxBlocks;
    // Number of ranges in sparse range index - 1 byte
    header[22] = m_sparseRanges.size();
	// Flags (unused & reserved, should be 0) - 1 byte
    header[23] = 0;

    // Timestamp based UUID - 8 bytes
    if (m_uniqueId == 0) {
        m_uniqueId = GetTime();
    }
    memcpy(&header[24], &m_uniqueId, sizeof(m_uniqueId));

	int writePos = V2FSEQ_HEADER_SIZE;

	// Empty compression blocks are automatically added when calculating headerSize (see maxBlocks)
	// Their data is initialized to 0 by memset and computed later
	writePos += maxBlocks * V2FSEQ_COMPRESSION_BLOCK_SIZE;

	// Sparse ranges
	for (auto &a : m_sparseRanges) {
		write3ByteUInt(&header[writePos], a.first);
		write3ByteUInt(&header[writePos + 3], a.second);
		writePos += V2FSEQ_SPARSE_RANGE_SIZE;
	}

	// Variable headers
    for (auto &a : m_variableHeaders) {
		uint32_t len = a.data.size() + V2FSEQ_VARIABLE_HEADER_SIZE;
		write2ByteUInt(&header[writePos], len);
		header[writePos + 2] = a.code[0];
		header[writePos + 3] = a.code[1];
		memcpy(&header[writePos + 4], &a.data[0], a.data.size());
		writePos += len;
    }

	// Write full header
	// This includes padding bytes to ensure m_seqChanDataOffset aligns with headerSize
	write(header, headerSize);

    LogDebug(VB_SEQUENCE, "Setup for writing v2 FSEQ\n");
    dumpInfo(true);
}


V2FSEQFile::V2FSEQFile(const std::string &fn, FILE *file, const std::vector<uint8_t> &header)
: FSEQFile(fn, file, header),
m_compressionType(none),
m_handler(nullptr)
{
    if (header[0] == 'E') {
        uint32_t modelLen = read4ByteUInt(&header[16]);
        uint32_t modelStart = read4ByteUInt(&header[12]);

        m_compressionType = CompressionType::none;
        //ESEQ files use 1 based start channels, we need 0 based
        m_sparseRanges.push_back(std::pair<uint32_t, uint32_t>(modelStart ? modelStart - 1 : modelStart, modelLen));
    } else {
        //24-31 - timestamp/uuid/identifier
        uint64_t *a = (uint64_t*)&header[24];
        m_uniqueId = *a;
        
        switch (header[20]) {
            case 0:
            m_compressionType = CompressionType::none;
            break;
            case 1:
            m_compressionType = CompressionType::zstd;
            break;
            case 2:
            m_compressionType = CompressionType::zlib;
            break;
            default:
            LogErr(VB_SEQUENCE, "Unknown compression type: %d", (int)header[20]);
        }
        
        uint32_t maxBlocks = header[21];
        
        uint64_t offset = m_seqChanDataOffset;
        int hoffset = V2FSEQ_HEADER_SIZE;
        for (int x = 0; x < maxBlocks; x++) {
            int frame = read4ByteUInt(&header[hoffset]);
            hoffset += 4;
            uint64_t dlen = read4ByteUInt(&header[hoffset]);
            hoffset += 4;
            if (dlen > 0) {
                m_frameOffsets.push_back(std::pair<uint32_t, uint64_t>(frame, offset));
                offset += dlen;
            }
            if (x == 0) {
                uint64_t doff = m_seqChanDataOffset;
                preload(doff, dlen);
            }
        }
        if (m_frameOffsets.size() == 0) {
            //this is bad... not sure what we can do.  We'll force a "0" block to
            //avoid a crash, but the data might not load correctly
            LogErr(VB_SEQUENCE, "FSEQ file corrupt: did not load any block references from header.");

            m_frameOffsets.push_back(std::pair<uint32_t, uint64_t>(0, offset));
            offset += this->m_seqFileSize - offset;
        }
        m_frameOffsets.push_back(std::pair<uint32_t, uint64_t>(getNumFrames() + 2, offset));
        //sparse ranges
        for (int x = 0; x < header[22]; x++) {
            uint32_t st = read3ByteUInt(&header[hoffset]);
            uint32_t len = read3ByteUInt(&header[hoffset + 3]);
            hoffset += 6;
            m_sparseRanges.push_back(std::pair<uint32_t, uint32_t>(st, len));
        }
        parseVariableHeaders(header, hoffset);
    }

    createHandler();
}
V2FSEQFile::~V2FSEQFile() {
    if (m_handler) {
        delete m_handler;
    }
}
void V2FSEQFile::dumpInfo(bool indent) {
    FSEQFile::dumpInfo(indent);
    char ind[5] = "    ";
    if (!indent) {
        ind[0] = 0;
    }

    LogDebug(VB_SEQUENCE, "%sSequence File Information\n", ind);
    LogDebug(VB_SEQUENCE, "%scompressionType       : %d\n", ind, m_compressionType);
    LogDebug(VB_SEQUENCE, "%snumBlocks             : %d\n", ind, m_handler->computeMaxBlocks());
    // Commented out to declutter the logs ... we can add it back in if we start seeing issues
    //for (auto &a : m_frameOffsets) {
    //    LogDebug(VB_SEQUENCE, "%s      %d              : %" PRIu64 "\n", ind, a.first, a.second);
    //}
    LogDebug(VB_SEQUENCE, "%snumRanges             : %d\n", ind, m_sparseRanges.size());
    // Commented out to declutter the logs ... we can add it back in if we start seeing issues
    //for (auto &a : m_sparseRanges) {
    //    LogDebug(VB_SEQUENCE, "%s      Start: %d    Len: %d\n", ind, a.first, a.second);
    //}
}

void V2FSEQFile::prepareRead(const std::vector<std::pair<uint32_t, uint32_t>> &ranges, uint32_t startFrame) {
    if (m_sparseRanges.empty()) {
        m_rangesToRead.clear();
        m_dataBlockSize = 0;
        for (auto rng : ranges) {
            //make sure we don't read beyond the end of the sequence data
            int toRead = rng.second;
            
            if (rng.first < m_seqChannelCount) {
                if ((rng.first + toRead) > m_seqChannelCount) {
                    toRead = m_seqChannelCount - rng.first;
                    rng.second = toRead;
                }
                m_dataBlockSize += toRead;
                m_rangesToRead.push_back(rng);
            }
        }
        if (m_dataBlockSize == 0) {
            m_rangesToRead.push_back(std::pair<uint32_t, uint32_t>(0, getMaxChannel() + 1));
            m_dataBlockSize = getMaxChannel();
        }
    } else if (m_compressionType != CompressionType::none) {
        //with compression, there is no way to NOT read the entire frame of data, we'll just
        //use the sparse data range since we'll have everything anyway so the ranges
        //needed is relatively irrelevant
        m_dataBlockSize = m_seqChannelCount;
        m_rangesToRead = m_sparseRanges;
    } else {
        //no compression with sparse ranges
        //FIXME - an intersection between the two would be useful, but hard
        //for now, just assume that if it's sparse, it has all the data that is needed
        //and read everything
        m_dataBlockSize = m_seqChannelCount;
        m_rangesToRead = m_sparseRanges;
    }
    m_handler->prepareRead(startFrame);
}
FrameData *V2FSEQFile::getFrame(uint32_t frame) {
    if (m_rangesToRead.empty()) {
        std::vector<std::pair<uint32_t, uint32_t>> range;
        range.push_back(std::pair<uint32_t, uint32_t>(0, getMaxChannel() + 1));
        prepareRead(range, frame);
    }
    if (frame >= m_seqNumFrames) {
        return nullptr;
    }
    if (m_handler != nullptr) {
        FrameData* fd = nullptr;
        try {
            fd = m_handler->getFrame(frame);
        } catch(...) {
            LogErr(VB_SEQUENCE, "Error getting frame from handler %s.\n", m_handler->GetType().c_str());
        }
        return fd;
    }
    return nullptr;
}
void V2FSEQFile::addFrame(uint32_t frame,
                          const uint8_t *data) {
    if (m_handler != nullptr) {
        m_handler->addFrame(frame, data);
    }
}

void V2FSEQFile::finalize() {
    if (m_handler != nullptr) {
        m_handler->finalize();
    }
    FSEQFile::finalize();
}

uint32_t V2FSEQFile::getMaxChannel() const {
    uint32_t ret = m_seqChannelCount;
    for (auto &a : m_sparseRanges) {
        uint32_t m = a.first + a.second - 1;
        if (m > ret) {
            ret = m;
        }
    }
    return ret;
}
