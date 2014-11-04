#include <iostream>
#include <list>
#include <map>
#include <set>
#include <vector>

#include "xLights/xLights/NetInfo.h"

using namespace std;

// Some defines from xLights' include/global.h
#define XTIMER_INTERVAL 50
#define XLIGHTS_SEQUENCE_EXT "xseq"

// Declare some vars needed by xLights conversion code here

typedef std::vector<unsigned char> SeqDataType;


NetInfoClass     NetInfo;
std::string      CurrentDir;
std::string      mediaFilename;
std::string      xlightsFilename;
SeqDataType      SeqData;
long             SeqDataLen;
long             SeqNumPeriods;
long             SeqNumChannels;
std::vector<std::string>  ChannelNames;
std::vector<int> ChannelColors;
std::set<int>    LorTimingList;


void AppendConvertLog(const std::string& msg)
{
    cout << "Log: " << (const char*)msg.c_str();
}

void AppendConvertStatus(const std::string& msg)
{
    cout << "Log: " << (const char*)msg.c_str();
}
bool mapEmptyChannels() {
    return false;
}
bool isSetOffAtEnd() {
    return false;
}

// Functions from xLightsMain.cpp needed by TabConvert.cpp
void PlayerError(const std::string& msg)
{
	cout << "Player Error: " << (const char*)msg.c_str() << endl;
}

double rand01(void)
{
	return (double)rand()/(double)RAND_MAX;
}

void AppendLog(const std::string& msg)
{
	cout << "Log: " << (const char*)msg.c_str();
}

// Functions to replace things we stripepd from TabConvert.cpp
void ConversionError(const std::string& msg)
{
	cout << "Conversion Error: " << (const char*)msg.c_str() << endl;
}

void SetStatusText(const std::string& msg) {
    cout << "Status: " << (const char*)msg.c_str() << endl;
}



#include <iostream>
#include <fstream>

#define FRAMECLASS
#define wxString std::string
void wxYield() {}

inline std::string string_format(const std::string fmt, ...) {
    int n, size=100;
    std::string str;
    va_list ap;
    while (1) {
        str.resize(size);
        va_start(ap, fmt);
        int n = vsnprintf((char *)str.c_str(), size, fmt.c_str(), ap);
        va_end(ap);
        if (n > -1 && n < size) {
            return str;
        }
        if (n > -1) {
            size=n+1;
        } else {
            size*=2;
        }
    }
}
typedef std::vector<int> wxArrayInt;
typedef std::vector<std::string> wxArrayString;

typedef signed char wxInt8;
typedef unsigned char wxUint8;
typedef wxUint8 wxByte;
typedef signed short wxInt16;
typedef unsigned short wxUint16;
typedef wxUint16 wxWord;
typedef unsigned long wxUint32;
class wxFile {
    std::string name;
    std::fstream stream;
    bool reading;
public:
    wxFile() {
    }
    wxFile(std::string n) {
        Open(n);
    }
    void Close() {
        stream.close();
    }
    void Write(const void *buf, size_t sz) {
        stream.write((const char *)buf, sz);
    }
    void Write(const std::string &buf) {
        Write(buf.c_str(), buf.size());
    }
    size_t Read(void *buf, size_t sz) {
        std::streampos cur = stream.tellg();
        if (cur == -1) {
            return 0;
        }
        stream.read((char *)buf, sz);
        std::streampos now = stream.tellg();
        return now - cur;
    }
    
    size_t Length() {
        if (reading) {
            std::streampos cur = stream.tellg();
            stream.seekg(0, std::ios::end);
            std::streampos ending_position = stream.tellg();
            stream.seekg(cur);
            return ending_position;
        }
        return stream.tellp();
    }
    bool Create(std::string n, bool x) {
        reading = false;
        name = n;
        stream.open(n, ios::in | ios::out | ios::binary | ios::trunc);
    }
    bool Open(std::string n) {
        reading = true;
        name = n;
        stream.open(n, ios::in | ios::out | ios::binary);
    }
    void Seek(size_t pos) {
        stream.clear();
        if (reading) {
            stream.seekg(pos, ios_base::beg);
        } else {
            stream.seekp(pos, ios_base::beg);
        }
    }
};
class wxFileName {
    std::string name;
public:
    wxFileName(std::string n) : name(n) {
    }
    
    std::string GetExt() {
        int idx = name.find_last_of(".");
        return name.substr(idx + 1);
    }
    void SetExt(std::string ext) {
        int idx = name.find_last_of(".");
        name = name.substr(0, idx + 1) + ext;
    }
    std::string GetFullPath() {
        return name;
    }
    void SetPath(std::string path) {
        int idx = name.find_last_of("/");
        if (idx != std::string::npos) {
            name = name.substr(idx + 1);
        }
        if (path.size() > 0) {
            name = path + "/" + name;
        }
    }
};

std::string FromAscii(const char *val) {
    return val;
}
void RemoveAt(wxArrayString &v, int i) {
    v.erase(v.begin()+i);
}


#include "xLights/xLights/TabConvert.cpp"

int main(int argc, char **argv)
{
	char *in = strdup("in.vix"); // for testing
	char *fmt = strdup("Falcon");

    int offset = 0;
    if (argc > 1) {
        if (argv[1][0] == '-') {
            long i2;
            std::string info(&argv[1][1]);
            while (info.find(",") != std::string::npos) {
                std::string t2 = info.substr(0, info.find(","));
                
                i2 = atol(t2.c_str());
                if ( i2 > 0) {
                    NetInfo.AddNetwork(i2);
                }
                info = info.substr(info.find(",") + 1);
            }
            i2 = atol(info.c_str());
            if ( i2 > 0) {
                NetInfo.AddNetwork(i2);
            }
            offset = 1;
        }
    }
    
	if (argc > (1 + offset)) {
		free(in);
		in = strdup(argv[1 + offset]);

		if (argc > (2 + offset)) {
			free(fmt);
			fmt = strdup(argv[2 + offset]);
		}
	}

	cout << "Converting '" << in << "' to '" << fmt << "' format" << endl;

    std::string inFilename(in);
    std::string outFormat(fmt);

	// Some read routines need a char ptr instead of a wxString
	//ReadVixFile(inFilename.char_str());
	//WriteFalconPiFile(outFilename);

	// Do it the xLights way
	DoConversion(inFilename, outFormat);

	free(in);
	free(fmt);

	exit(0);
}

