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

// Include our script-modified version of xLights' TabConvert.cpp code
#include "xLights_TabConvert.h"
#include "xLights_TabConvert.cpp"

// Functions to replace things we stripepd from TabConvert.cpp
void ConversionError(const wxString& msg)
{
	cout << "Conversion Error: " << (const char*)msg.c_str() << endl;
}

int main(int argc, char **argv)
{
	char *in = strdup("in.vix"); // for testing
	char *fmt = strdup("Falcon");

	if (argc > 1) {
		free(in);
		in = strdup(argv[1]);

		if (argc > 2) {
			free(fmt);
			fmt = strdup(argv[2]);
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

