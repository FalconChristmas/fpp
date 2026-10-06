#pragma once

// PLUGIN ABI: native plugins compile against this header and are loaded with
// dlopen(), so parts of it are baked into binaries FPP did not build.
//   - FSEQFile, V1FSEQFile, V2FSEQFile and FrameData vtables: plugins call
//     their virtuals (a mid-vtable virtual added here once only escaped
//     because an unrelated bump landed a week later).
//   - Their data members: plugins use inline setters that write protected
//     fields, and read V2FSEQFile::m_sparseRanges.
//   - VariableHeader, which plugins construct and push by value.
// Any such change must bump FPP_PLUGIN_API_VERSION in Plugin.h. Without the
// bump, a plugin built against the previous header still loads and silently
// uses the old layout - FPP 10.2 shipped exactly that and crash-looped fppd.
// See .claude/PLUGIN-ABI.md for which plugins use what.

#include <stdio.h>
#include <cstdint>
#include <string>
#include <vector>

class FSEQFile {
public:
    class VariableHeader {
    public:
        VariableHeader() :
            fseqFile(nullptr) {
            code[0] = code[1] = 0;
            offset = 0;
            length = 0;
            extendedData = false;
        }
        VariableHeader(FSEQFile* fseq) :
            fseqFile(fseq),
            offset(0),
            length(0) {
            code[0] = code[1] = 0;
            extendedData = false;
        }
        VariableHeader(const VariableHeader& cp) :
            data(cp.data) {
            code[0] = cp.code[0];
            code[1] = cp.code[1];
            extendedData = cp.extendedData;
            offset = cp.offset;
            length = cp.length;
            fseqFile = cp.fseqFile;
        }
        ~VariableHeader() {}

        uint8_t code[2];
        bool extendedData = false;

        void resizeData(size_t sz) {
            data.resize(sz);
            length = 0;
            offset = 0;
        }
        const uint32_t getDataLength() const {
            if (!data.empty()) {
                return (uint32_t)data.size();
            }
            return length;
        }
        void setDataLocation(uint64_t off, uint32_t len) {
            offset = off;
            length = len;
        }
        const std::vector<uint8_t>& getData() const {
            loadData();
            return data;
        }
        std::vector<uint8_t>& getData() {
            loadData();
            return data;
        }

        uint64_t getExtDataOffset() const { return extendedData ? offset : 0; }

    private:
        void loadData() const;
        mutable std::vector<uint8_t> data;
        uint64_t offset;
        uint32_t length;
        FSEQFile* fseqFile;
    };

    class FrameData {
    public:
        FrameData(uint32_t f) :
            frame(f){};
        virtual ~FrameData(){};

        virtual bool readFrame(uint8_t* data, uint32_t maxChannels) = 0;

        uint32_t frame;
    };

    enum CompressionType {
        none,
        zstd,
        zlib
    };
    constexpr static const char* CompressionTypeStrings[] = { "none", "zstd", "zlib" };

protected:
    // open file for reading
    FSEQFile(const std::string& fn, FILE* file, const std::vector<uint8_t>& header);
    // open file for writing
    FSEQFile(const std::string& fn);

public:
    virtual ~FSEQFile();

    static FSEQFile* openFSEQFile(const std::string& fn);

    static FSEQFile* createFSEQFile(const std::string& fn,
                                    int version,
                                    CompressionType ct = CompressionType::zstd,
                                    int level = -99);
    // utility methods
    static std::string getMediaFilename(const std::string& fn);
    std::string getMediaFilename() const;
    uint32_t getTotalTimeMS() const { return m_seqNumFrames * m_seqStepTime; }

    void parseVariableHeaders(const std::vector<uint8_t>& header, int start);

    // How the caller intends to read this file.  This is about the access
    // pattern, not the hardware: it says what the reader is allowed to do ahead
    // of the frame being asked for.
    enum class ReadPattern {
        // Serve one frame at a time with the smallest possible latency and
        // memory footprint - what playback needs, especially on a small single
        // core device that cannot hold the sequence.  The default.
        Streaming,
        // Every frame will be read in order, front to back, as fast as
        // possible, and the caller has already allocated room for what it
        // needs.  The reader may then decompress whole blocks ahead of the
        // requested frame, in parallel, at the cost of a bounded amount of
        // extra memory.
        Bulk
    };
    // Must be set before prepareRead()/getFrame() to have any effect.
    virtual void setReadPattern(ReadPattern p) { m_readPattern = p; }
    ReadPattern getReadPattern() const { return m_readPattern; }

    // prepare to start reading. The ranges will be the list of channel ranges that
    // are actually needed for each frame.   The reader can optimize to only
    // read those frames.
    virtual void prepareRead(const std::vector<std::pair<uint32_t, uint32_t>>& ranges, uint32_t startFrame = 0) {}

    // For reading data from the fseq file, returns an object can
    // provide the necessary data in a timely fashion for the given frame
    // It may not be used right away and will be deleted at some point in the future
    virtual FrameData* getFrame(uint32_t frame) = 0;

    // For writing to the fseq file
    virtual void enableMinorVersionFeatures(uint8_t ver) {}
    virtual void initializeFromFSEQ(const FSEQFile& fseq);
    virtual void writeHeader() = 0;
    virtual void addFrame(uint32_t frame,
                          const uint8_t* data) = 0;
    virtual void finalize();

    virtual void dumpInfo(bool indent = false);

    uint32_t getNumFrames() const { return m_seqNumFrames; }
    int getStepTime() const { return m_seqStepTime; }
    uint32_t getChannelCount() const { return m_seqChannelCount; }
    int getVersionMajor() const { return m_seqVersionMajor; }
    int getVersionMinor() const { return m_seqVersionMinor; }
    uint64_t getUniqueId() const { return m_uniqueId; }
    const std::string& getFilename() const { return m_filename; }

    virtual uint32_t getMaxChannel() const = 0;
    const std::vector<VariableHeader>& getVariableHeaders() const { return m_variableHeaders; }

    void setNumFrames(uint32_t f) { m_seqNumFrames = f; }
    void setStepTime(int st) { m_seqStepTime = st; }
    void setChannelCount(int cc) { m_seqChannelCount = cc; }
    void addVariableHeader(const VariableHeader& header) { m_variableHeaders.push_back(header); }

    const std::vector<uint8_t>& getMemoryBuffer() const { return m_memoryBuffer; }
    uint64_t getMemoryBufferPos() const { return m_memoryBufferPos; }

protected:
    std::string m_filename;
    uint64_t m_uniqueId;
    uint32_t m_seqNumFrames;
    uint32_t m_seqChannelCount;
    int m_seqStepTime;
    int m_seqVersionMajor;
    int m_seqVersionMinor;
    ReadPattern m_readPattern = ReadPattern::Streaming;

    std::vector<VariableHeader> m_variableHeaders;

protected:
    uint64_t m_seqFileSize;
    uint64_t m_seqChanDataOffset;

    int seek(uint64_t location, int origin);
    uint64_t tell();
    uint64_t write(const void* ptr, uint64_t size);
    uint64_t read(void* ptr, uint64_t size);
    void preload(uint64_t pos, uint64_t size);

private:
    FILE* volatile m_seqFile;
    std::vector<uint8_t> m_memoryBuffer;
    uint64_t m_memoryBufferPos;
};

class V1FSEQFile : public FSEQFile {
public:
    V1FSEQFile(const std::string& fn, FILE* file, const std::vector<uint8_t>& header);
    V1FSEQFile(const std::string& fn);

    virtual ~V1FSEQFile();

    virtual void prepareRead(const std::vector<std::pair<uint32_t, uint32_t>>& ranges, uint32_t startFrame = 0) override;
    virtual FrameData* getFrame(uint32_t frame) override;

    virtual void writeHeader() override;
    virtual void addFrame(uint32_t frame,
                          const uint8_t* data) override;
    virtual void finalize() override;

    virtual uint32_t getMaxChannel() const override;

    // The ranges to read and the data size needed to read the ranges
    std::vector<std::pair<uint32_t, uint32_t>> m_rangesToRead;
    uint32_t m_dataBlockSize;
};

class V2Handler;

class V2FSEQFile : public FSEQFile {
public:
    V2FSEQFile(const std::string& fn, FILE* file, const std::vector<uint8_t>& header);
    V2FSEQFile(const std::string& fn, CompressionType ct, int cl);

    virtual ~V2FSEQFile();

    virtual void prepareRead(const std::vector<std::pair<uint32_t, uint32_t>>& ranges, uint32_t startFrame = 0) override;
    virtual FrameData* getFrame(uint32_t frame) override;

    virtual void writeHeader() override;
    virtual void addFrame(uint32_t frame,
                          const uint8_t* data) override;
    virtual void finalize() override;

    virtual void dumpInfo(bool indent = false) override;

    virtual uint32_t getMaxChannel() const override;

    virtual void enableMinorVersionFeatures(uint8_t ver) override {
        m_seqVersionMinor = ver;
        if (ver == 0) {
            m_allowExtendedBlocks = false;
        }
        if (ver >= 1) {
            m_allowExtendedBlocks = true;
        }
    }

    [[nodiscard]] std::string CompressionTypeString() const {
        return CompressionTypeStrings[(int)m_compressionType];
    }

    CompressionType m_compressionType;
    int m_compressionLevel;
    std::vector<std::pair<uint32_t, uint32_t>> m_sparseRanges;
    std::vector<std::pair<uint32_t, uint32_t>> m_rangesToRead;
    std::vector<std::pair<uint32_t, uint64_t>> m_frameOffsets;
    uint32_t m_dataBlockSize;
    bool m_allowExtendedBlocks;

private:
    void createHandler();

    V2Handler* m_handler;
    friend class V2Handler;
};
