#include <iostream>
#include <list>
#include <map>
#include <set>
#include <vector>

#include <wx/checklst.h>
#include <wx/config.h>
#include <wx/event.h>
#include <wx/file.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/statusbr.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/xml/xml.h>

#include "xLights/xLights/NetInfo.h"

using namespace std;

// Some defines from xLights' include/global.h
#define XTIMER_INTERVAL 50
#define XLIGHTS_SEQUENCE_EXT "xseq"

// Declare some vars needed by xLights conversion code here

typedef std::vector<wxUint8> SeqDataType;


NetInfoClass     NetInfo;
wxString         CurrentDir;
wxString         mediaFilename;
wxString         xlightsFilename;
SeqDataType      SeqData;
long             SeqDataLen;
long             SeqNumPeriods;
long             SeqNumChannels;
wxArrayString    ChannelNames;
wxArrayInt       ChannelColors;
std::set<int>    LorTimingList;

// FIXME, need to do something about these before we use an unitialized pointer
wxTextCtrl*      TextCtrlLog;
wxCheckListBox*  CheckListBoxTestChannels;
wxStatusBar*     StatusBar1;

void AppendConvertLog(const wxString& msg)
{
    cout << "Log: " << (const char*)msg.c_str();
}

void AppendConvertStatus(const wxString& msg)
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
void PlayerError(const wxString& msg)
{
	cout << "Player Error: " << (const char*)msg.c_str() << endl;
}

double rand01(void)
{
	return (double)rand()/(double)RAND_MAX;
}

void AppendLog(const wxString& msg)
{
	cout << "Log: " << (const char*)msg.c_str();
}

// Functions to replace things we stripepd from TabConvert.cpp
void ConversionError(const wxString& msg)
{
	cout << "Conversion Error: " << (const char*)msg.c_str() << endl;
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
            wxString info(&argv[1][1]);
            while (info.Contains(",")) {
                wxString t2 = info.SubString(0, info.Find(","));
                
                t2.ToLong(&i2);
                if ( i2 > 0) {
                    NetInfo.AddNetwork(i2);
                }
                info = info.SubString(info.Find(",") + 1, info.length());
            }
            i2 = 0;
            info.ToLong(&i2);
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

	wxString inFilename  = wxString::Format(_("%s"), in);
	wxString outFormat = wxString::Format(_("%s"), fmt);

	// Some read routines need a char ptr instead of a wxString
	//ReadVixFile(inFilename.char_str());
	//WriteFalconPiFile(outFilename);

	// Do it the xLights way
	DoConversion(inFilename, outFormat);

	free(in);
	free(fmt);

	exit(0);
}

