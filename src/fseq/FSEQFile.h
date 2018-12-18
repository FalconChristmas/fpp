#ifndef __FSEQFILE_H_

#include <stdio.h>
#include <string>
#include <vector>

class VariableHeader {
public:
    VariableHeader() { code[0] = code[1] = 0; }
    VariableHeader(const VariableHeader& cp) : data(cp.data) {
        code[0] = cp.code[0];
        code[1] = cp.code[1];
    }
    ~VariableHeader() {}
    
    uint8_t code[2];
    std::vector<uint8_t> data;
};


class FrameData {
public:
    FrameData(uint32_t f) : frame(f) {};
    virtual ~FrameData() {};
    
    virtual void readFrame(uint8_t *data) = 0;
    
    uint32_t frame;
};


class FSEQFile {
public:
    enum CompressionType {
        none,
        zstd,
        zlib
    };

protected:
    //open file for reading
    FSEQFile(const std::string &fn, FILE *file, const std::vector<uint8_t> &header);
    //open file for writing
    FSEQFile(const std::string &fn);
    
public:
    
    virtual ~FSEQFile();
    
    static FSEQFile* openFSEQFile(const std::string &fn);
    
    static FSEQFile* createFSEQFile(const std::string &fn,
                                    int version,
                                    CompressionType ct = CompressionType::zstd,
                                    int level = 10);
    
    void parseVariableHeaders(const std::vector<uint8_t> &header, int start);
    
    
    //prepare to start reading. The ranges will be the list of channel ranges that
    //are acutally needed for each frame.   The reader can optimize to only
    //read those frames.
    virtual void prepareRead(const std::vector<std::pair<uint32_t, uint32_t>> &ranges) {}
    
    //For reading data from the fseq file, returns an object can
    //provide the necessary data in a timely fassion for the given frame
    //It may not be used right away and will be deleted at some point in the future
    virtual FrameData *getFrame(uint32_t frame) = 0;
    
    //For writing to the fseq file
    virtual void initializeFromFSEQ(const FSEQFile& fseq);
    virtual void writeHeader() = 0;
    virtual void addFrame(uint32_t frame,
                          uint8_t *data) = 0;
    virtual void finalize() = 0;
    
    virtual void dumpInfo(bool indent = false);
    
    std::string filename;
    
    FILE* volatile  m_seqFile;
    
    uint64_t      m_seqFileSize;
    uint64_t      m_seqChanDataOffset;
    uint64_t      m_uniqueId;

    uint32_t      m_seqNumFrames;
    uint32_t      m_seqChannelCount;
    int           m_seqStepTime;
    int           m_seqVersionMajor;
    int           m_seqVersionMinor;
    int           m_seqVersion;
    
    std::vector<VariableHeader> m_variableHeaders;
};


class V1FSEQFile : public FSEQFile {
public:
    V1FSEQFile(const std::string &fn, FILE *file, const std::vector<uint8_t> &header);
    V1FSEQFile(const std::string &fn);

    virtual ~V1FSEQFile();
  
    virtual void prepareRead(const std::vector<std::pair<uint32_t, uint32_t>> &ranges) override;
    virtual FrameData *getFrame(uint32_t frame) override;

    virtual void writeHeader() override;
    virtual void addFrame(uint32_t frame,
                          uint8_t *data) override;
    virtual void finalize() override;
    
    //The ranges to read and the data size needed to read the ranges
    std::vector<std::pair<uint32_t, uint32_t>> m_rangesToRead;
    uint32_t m_dataBlockSize;
};


class V2Handler;

class V2FSEQFile : public FSEQFile {

public:
    V2FSEQFile(const std::string &fn, FILE *file, const std::vector<uint8_t> &header);
    V2FSEQFile(const std::string &fn, CompressionType ct, int cl);

    virtual ~V2FSEQFile();
    
    virtual void prepareRead(const std::vector<std::pair<uint32_t, uint32_t>> &ranges) override;
    virtual FrameData *getFrame(uint32_t frame) override;
    
    virtual void writeHeader() override;
    virtual void addFrame(uint32_t frame,
                          uint8_t *data) override;
    virtual void finalize() override;

    virtual void dumpInfo(bool indent = false) override;

    
    CompressionType m_compressionType;
    int             m_compressionLevel;
    std::vector<std::pair<uint32_t, uint32_t>> m_sparseRanges;
    std::vector<std::pair<uint32_t, uint32_t>> m_rangesToRead;
    std::vector<std::pair<uint32_t, uint64_t>> m_frameOffsets;
    uint32_t m_dataBlockSize;
private:
    
    void createHandler();
    
    V2Handler *m_handler;
};


#endif
