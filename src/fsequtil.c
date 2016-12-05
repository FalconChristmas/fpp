/*
 *   fsequtil Utility for the Falcon Player (FPP) 
 *
 *   Copyright (C) 2013 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "fppversion.h"
#include "log.h"
#include "SequenceFile.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


void Usage(char *appname);


/*
 *
 */
int fseqInfo(char *filename)
{
	SequenceFile seqFile;

	seqFile.Open(filename);

	return 1;
}

// Some settings which will affect file copies
int brightness = 0;

/*
 *
 */
int fseqCopy(char *inFilename, char *outFilename)
{
	SequenceFile inFile;
	SequenceFile outFile;

	inFile.Open(inFilename);
	outFile.Open(outFilename, &inFile);

	outFile.ScaleBrightness(brightness);

	outFile.Copy(&inFile);

	return 1;
}


/*
 *
 */
int main (int argc, char *argv[])
{
	char *inFilename = 0;
	char *outFilename = 0;
	int info = 0;

	SetLogLevel("debug");
	SetLogMask("sequence");

	if(argc>1)
	{
		for (int c = 1; c < argc; c++)
		{
			// Info - example "fpp -i <FILENAME>"
			if(!strncmp(argv[c],"-i",2))
			{
				info = 1;
			}
			else if(!strncmp(argv[c],"-o",2) && ((c+1) < argc))
			{
				outFilename = argv[c+1];
				c++;
			}
			else if(!strncmp(argv[c],"-b",2) && ((c+1) < argc))
			{
				brightness = atoi(argv[c+1]);
				c++;
			}
			else if (c == (argc - 1))
			{
				inFilename = argv[c];
			}
			else
			{
				// Unknown option, print usage
				printf( "Unknown option: '%s'\n\n", argv[c]);

				Usage(argv[0]);
			}
		}
	}
	else
	{
		Usage(argv[0]);
	}

	if (!inFilename)
	{
		Usage(argv[0]);
	}

	if (info)
	{
		fseqInfo(inFilename);
	}
	else if (outFilename)
	{
		fseqCopy(inFilename, outFilename);
	}
	else
	{
		// No other valid args so display info on the file by default
		fseqInfo(inFilename);
	}

	return 0;
}

/*
 *
 */
void Usage(char *appname)
{
	printf("Usage: %s [OPTION...] FILENAME\n"
"\n"
"fsequtil is the Falcon Player FSEQ file helper utility.  It can be used to \n"
"manipulate FSEQ sequence files in various ways.\n"
"\n"
"Options:\n"
"  -V                       - Print version information\n"
"  -i                       - Get info on a sequence file\n"
"  -o OUTPUT_FILENAME       - Specify output FSEQ filename\n"
"  -f                       - Force overwrite of existing file\n"
"  -b PERCENT               - Scale output to PERCENT of input\n"
"  -s RANGE[,RANGE2...]     - Create a Sparse FSEQ of given channel ranges\n"
"  -d [START[-END]]         - Dump channel data (optional start/end range)\n"
"  -S STARTFRAME            - Only copy data starting at STARTFRAME frame #\n"
"  -E ENDFRAME              - Only copy data ending at ENDFRAME frame #\n"
"\n", appname);

	exit(0);
}
