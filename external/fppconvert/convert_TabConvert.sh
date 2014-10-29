#!/bin/sh

echo "// Prototypes for functions in xLights' TabConvert.cpp" \
	> xLights_TabConvert.h

grep -A 1 xLightsFrame:: xLights/xLights/TabConvert.cpp \
	| grep -v -- "--" \
	| grep -v "{" \
	| sed \
		-e "s/xLightsFrame:://" \
		-e "s/).*$/);/" \
	>> xLights_TabConvert.h

cat xLights/xLights/TabConvert.cpp \
	| ./strip_function.pl xLightsFrame::ConversionError \
	| ./strip_function.pl xLightsFrame::OnButtonChooseFileClick \
	| ./strip_function.pl xLightsFrame::OnButtonStartConversionClick \
	| ./strip_function.pl xLightsFrame::ReadConductorFile \
	| ./strip_function.pl xLightsFrame::WriteConductorFile \
	| grep -v "#include.*xLightsMain\.h" \
	| sed \
		-e "s#../include/spxml#xLights/include/spxml#" \
		-e "s/xLightsFrame:://" \
		-e "s/Timer1\.GetInterval()/50/" \
		-e "s/TextCtrlConversionStatus.*AppendText/AppendLog/" \
		-e "s/TextCtrlLog.*AppendText/AppendLog/" \
		-e "s/CheckBoxMapEmptyChannels.*$/true;/" \
		-e "s/CheckBoxOffAtEnd.*)$/false)/" \
		-e "s/ReadConductorFile.*$//" \
		-e "s/WriteConductorFile.*$//" \
	> xLights_TabConvert.cpp

