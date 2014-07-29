#
#   Falcon Pi Player memory mapped file interface access module
#   Falcon Pi Player project (FPP)
#
#   Copyright (C) 2013 the Falcon Pi Player Developers
#      Initial development by:
#      - David Pitts (dpitts)
#      - Tony Mace (MyKroFt)
#      - Mathew Mrosko (Materdaddy)
#      - Chris Pinkham (CaptainMurdoch)
#      For additional credits and developers, see credits.php.
#
#   The Falcon Pi Player (FPP) is free software; you can redistribute it
#   and/or modify it under the terms of the GNU General Public License
#   as published by the Free Software Foundation; either version 2 of
#   the License, or (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, see <http://www.gnu.org/licenses/>.
#

{
package FPP::MemoryMap;

use strict;
use threads;
use threads::shared;
use FindBin qw/$Bin/;
use Time::HiRes qw( usleep );
use File::Map qw/map_file unmap/;
use Convert::Binary::C;
use Data::Dumper;
use Image::Magick;

#############################################################################
# Usage:
#
# use lib "/opt/fpp/lib/perl";
#
# my $fppmm = new FPP::MemoryMap;
#
# my $testMode = $fppmm->GetTestMode();
# $fppmm->SetTestMode(1);
# $fppmm->SetTestMode(0);
#
#############################################################################
sub new {
	my $proto = shift;
	my $class = ref ($proto) || $proto;
	my (%argv) = @_;
	my $this = bless {
		majorVersion  => 1,
		minorVersion  => 0,
		debug         => 0,
		maxChannels   => 65536,
		memoryMapSize => 65536,
		ctrlFile      => '/var/tmp/FPPChannelCtrl',
		dataFile      => '/var/tmp/FPPChannelData',
		pixelFile     => '/var/tmp/FPPChannelPixelMap',
		}, $proto;

	$this->{C} = new Convert::Binary::C;
	$this->{C}->parse_file("/opt/fpp/src/memorymapcontrol.h");

	$this->{C}->tag('FPPChannelMemoryMapControlHeader.filler',
		Format => 'String');
	$this->{C}->tag('FPPChannelMemoryMapControlBlock.blockName',
		Format => 'String');
	$this->{C}->tag('FPPChannelMemoryMapControlBlock.startCorner',
		Format => 'String');
	$this->{C}->tag('FPPChannelMemoryMapControlBlock.orientation',
		Format => 'String');
	$this->{C}->tag('FPPChannelMemoryMapControlBlock.filler',
		Format => 'String');

	return $this;
}

#############################################################################
# Open the memory mapped files created by fppd
sub OpenMaps {
	my $this = shift;

	$this->CloseMaps();

	my $ctrlFileMap;
	my $dataFileMap;
	my $pixelFileMap;

	map_file $ctrlFileMap, $this->{ctrlFile}, '+<';
	map_file $dataFileMap, $this->{dataFile}, '+<';
	map_file $pixelFileMap, $this->{pixelFile};

	if (!$ctrlFileMap) {
		printf( STDERR "Unable to map control file: %s\n", $this->{ctrlFile});
		exit(-1);
	}

	if (!$dataFileMap) {
		printf( STDERR "Unable to map data file: %s\n", $this->{dataFile});
		exit(-1);
	}

	if (!$pixelFileMap) {
		printf( STDERR "Unable to map pixel file: %s\n", $this->{pixelFile});
		exit(-1);
	}

	$this->{ctrlFileMap} = \$ctrlFileMap;
	$this->{dataFileMap} = \$dataFileMap;
	$this->{pixelFileMap} = \$pixelFileMap;

	my @pixelMap = unpack('v' . $this->{maxChannels}, ${$this->{pixelFileMap}});
	$this->{pixelMap} = \@pixelMap;

	$this->ReadBlockInfo();
}

#############################################################################
# Close the memory mapped files
sub CloseMaps {
	my $this = shift;

	if (defined($this->{ctrlFileMap})) {
		delete $this->{ctrlFileMap};
	}

	if (defined($this->{dataFileMap})) {
		delete $this->{dataFileMap};
	}

	if (defined($this->{pixelFileMap})) {
		delete $this->{pixelFileMap};
	}

	if (defined($this->{blocks})) {
		delete $this->{blocks};
	}
}

#############################################################################
# Load a single block's info
sub LoadBlockInfo {
	my $this = shift;
	my $blk = shift;

	$blk->{data} = $this->{C}->unpack('FPPChannelMemoryMapControlBlock', substr(${$this->{ctrlFileMap}}, $blk->{offset}, 256));
}

#############################################################################
# Save a single block's info
sub SaveBlockInfo {
	my $this = shift;
	my $blk = shift;

	substr(${$this->{ctrlFileMap}}, $blk->{offset}, 256, $this->{C}->pack('FPPChannelMemoryMapControlBlock', $blk->{data}));
}

#############################################################################
# Read the blocks info
sub ReadBlockInfo {
	my $this = shift;
	my %blocks;
	$this->{Blocks} = \%blocks;

	my $ctrlHeader = $this->{C}->unpack('FPPChannelMemoryMapControlHeader', substr(${$this->{ctrlFileMap}}, 0, 256));

	$this->{MajorVersion} = $ctrlHeader->{majorVersion};
	$this->{MinorVersion} = $ctrlHeader->{minorVersion};
	$this->{TotalBlocks} = $ctrlHeader->{totalBlocks};
	$this->{TestMode} = $ctrlHeader->{testMode};
	$this->{ControlHeader} = $ctrlHeader;

	for (my $b = 0; $b < $this->{TotalBlocks}; $b++) {
		my $blkInfo = $this->{C}->unpack('FPPChannelMemoryMapControlBlock', substr(${$this->{ctrlFileMap}}, ($b+1)*256, 256));

		my %data;
		$data{blockName} = $blkInfo->{blockName};
		$data{bid} = $b;
		$data{data} = $blkInfo;
		$data{offset} = ($b+1)*256;

		$this->{Blocks}->{$blkInfo->{blockName}} = \%data;
	}
}

#############################################################################
# Get a single byte from a memory mapped file
sub GetChar {
	my $this = shift;
	my $map = shift;
	my $index = shift;

	my $c = substr(${$this->{$map . "FileMap"}}, $index, 1);
	return ord($c);
}

#############################################################################
# Update a single byte in a memory mapped file
sub SetChar {
	my $this = shift;
	my $map = shift;
	my $index = shift;
	my $value = shift;

	substr(${$this->{$map . "FileMap"}}, $index, 1, chr($value));
}

#############################################################################
# Get Test Mode
sub GetTestMode {
	my $this = shift;

	return $this->GetChar("ctrl", 3);
}

#############################################################################
# Enable/Disable Test Mode
sub SetTestMode {
	my $this = shift;
	my $state = shift || 0;

	if ($state != 0) {
		$this->SetChar('ctrl', 3, 1);
	} else {
		$this->SetChar('ctrl', 3, 0);
	}
}

#############################################################################
# Enable/Disable Test Mode
sub SetBlockState {
	my $this = shift;
	my $blk = shift;
	my $state = shift || 0;

	if ($state != 0) {
		$blk->{data}->{isActive} = 1;
	} else {
		$blk->{data}->{isActive} = 0;
	}

	$this->SaveBlockInfo($blk);
}

#############################################################################
# Enable/Disable Block Lock
sub SetBlockLock {
	my $this = shift;
	my $blk = shift;
	my $state = shift || 0;

	if ($state != 0) {
		$blk->{data}->{isLocked} = 1;
	} else {
		$blk->{data}->{isLocked} = 0;
	}

	$this->SaveBlockInfo($blk);
}

#############################################################################
# Locking functions
sub IsBlockLocked {
	my $this = shift;
	my $blk = shift;
	my $bd = $blk->{data};

	$this->LoadBlockInfo($blk);

	return $bd->{isLocked};
}

#############################################################################
# Set Absolute Pixel
sub SetPixel {
	my $this = shift;
	my $a = shift;
	my $r = shift;
	my $g = shift;
	my $b = shift;

	$this->SetChar('data', $this->{pixelMap}[$a], $r);
	$this->SetChar('data', $this->{pixelMap}[$a+1], $g);
	$this->SetChar('data', $this->{pixelMap}[$a+2], $b);
}

#############################################################################
# Set value of a pixel in a block
sub SetBlockPixel {
	my $this = shift;
	my $blk = shift;
	my $x = shift;
	my $y = shift;
	my $r = shift;
	my $g = shift;
	my $b = shift;
	my $bd = $blk->{data};

	my $width = $bd->{channelCount} / $bd->{strandsPerString} / $bd->{stringCount} / 3;

	my $a = ($y * $width + $x) * 3;

	$this->SetPixel($bd->{startChannel} - 1 + $a, $r, $g, $b);
}

#############################################################################
# Set a whole block to a specific color
sub SetBlockColor {
	my $this = shift;
	my $blk = shift;
	my $r = shift;
	my $g = shift;
	my $b = shift;

	my $pixels = $blk->{data}->{channelCount} / 3;
	for (my $i = $blk->{data}->{startChannel} - 1, my $p = 0; $p < $pixels; $p++, $i += 3)
	{
		$this->SetPixel($i, $r, $g, $b);
	}
}

#############################################################################
# Dump the Memory Map blocks config info
sub DumpConfig {
	my $this = shift;

	printf( "Config Version: %d.%d\n",
		$this->{MajorVersion}, $this->{MinorVersion});
	printf( "Total Blocks  : %d\n", $this->{TotalBlocks});
	printf( "Test Mode     : %d\n", $this->{TestMode});

	my $i = 1;
	foreach my $key ( keys %{$this->{Blocks}}) {
		my $b = $this->{Blocks}->{$key};
		printf( "Block #%d\n", $i++);
		printf( "    Name                : %s\n", $b->{data}->{blockName});
		printf( "    Enabled             : %s\n",
			$b->{data}->{isActive} ? "1 (Yes)": "0 (No)");
		printf( "    Start Channel       : %d\n", $b->{data}->{startChannel});
		printf( "    Channel Count       : %d\n", $b->{data}->{channelCount});
		printf( "    Orientation         : %s\n",
			$b->{data}->{orientation} eq "H" ? "H (Horizontal)" : "V (Vertical)");
		printf( "    Start Corner        : %s\n",
			$b->{data}->{startCorner} eq "TL" ? "TL (Top Left)"
			: $b->{data}->{startCorner} eq "TR" ? "TR (Top Right)"
			: $b->{data}->{startCorner} eq "BL" ? "BL (Bottom Left)"
			: "BR (Bottom Right)");
		printf( "    String Count        : %d\n", $b->{data}->{stringCount});
		printf( "    Strands Per String  : %d\n", $b->{data}->{strandsPerString});
	}
}

#############################################################################
# Get Block List
sub GetBlockList {
	my $this = shift;
	my %result;

	foreach my $key ( keys %{$this->{Blocks}}) {
		my $b = $this->{Blocks}->{$key};
		$result{$b->{blockName}} = $b->{data};
	}

	return \%result;
}

#############################################################################
# Get info about a specific block
sub GetBlockInfo {
	my $this = shift;
	my $name = shift;

	if (defined($this->{Blocks}->{$name})) {
		return $this->{Blocks}->{$name};
	}

	return undef;
}

#############################################################################
# Get block channel data in Top-Left line-wrapped order: 0 1 2
#                                                        3 4 5
#                                                        6 7 8
# Input Data is supplied in in R,G,B,R,G,B,... order
sub GetBlockData {
	my $this = shift;
	my $blk = shift;
	my $bd = $blk->{data};
	my @data;
	my $channels = $bd->{channelCount};

	for (my $i = $bd->{startChannel} - 1, my $c = 0; $c < $channels; $i++, $c++)
	{
		push( @data, $this->GetChar('data', $this->{pixelMap}[$i]));
	}

	return \@data;
}

#############################################################################
# Set block channel data in Top-Left line-wrapped order: 0 1 2
#                                                        3 4 5
#                                                        6 7 8
# Returned Data is in R,G,B,R,G,B,... order
sub SetBlockData {
	my $this = shift;
	my $blk = shift;
	my $data = shift;
	my $bd = $blk->{data};
	my $channels = $bd->{channelCount};

	for (my $i = $bd->{startChannel} - 1, my $c = 0; $c < $channels; $i++, $c++)
	{
		$this->SetChar('data', $this->{pixelMap}[$i], $data->[$c]);
	}
}

#############################################################################
# Get the list of available fonts
sub GetFontList {
	my $this = shift;

	my $img = Image::Magick->new();
	my @fonts = sort $img->QueryFont();

	return \@fonts;
}

#############################################################################
# Put a text message up on the display
sub TextMessage {
	my $this = shift;
	my $blk = shift;
	my $msg = shift;
	my $color = shift || "#ff0000";
	my $fill = shift || "#000000";
	my $font = shift || "Courier";
	my $size = shift || 10;
	my $pos = shift || "scroll";
	my $dir = shift || "R2L";
	my $pps = shift || 5;

	$this->SetBlockLock($blk, 1);

	my $bd = $blk->{data};
	my $sleepTime = 1000000 / $pps;
	my $width = $bd->{channelCount} / $bd->{strandsPerString} / $bd->{stringCount} / 3;
	my $height = $bd->{channelCount} / 3 / $width;
	my $img = Image::Magick->new(size => sprintf( "%dx%d", $width, $height));
	$img->Set(magick => "rgb");    # so we can save to blob as RGB
	$img->Set(depth => 8);         # needed for RGB data

	if (($color !~ /^#/) &&
		($color =~ /^[0-9]+$/)) {
		$color = "#" . $color;
	}

	if (($fill !~ /^#/) &&
		($fill =~ /^[0-9]+$/)) {
		$fill = "#" . $fill;
	}

	my $createImage;
	$createImage = sub {
		my $x = shift;
		my $y = shift;
		my $err;

		@$img = (); # clear the image

		$img->ReadImage('xc:' . $fill); # black/blank background
		$err = $img->Annotate(
				pointsize => $size,
				text => $msg,
				gravity=>'NorthWest',
				stroke => 'none',
				weight => 1,
				antialias => 0,
				font => $font,
				fill => $color,
				x => $x,
				y => $y,
			);
		print $err if $err;
	};

	my $overlayImage;
	$overlayImage = sub {
		my $rgbData = $img->ImageToBlob();
		my $channels = $bd->{channelCount};
		for (my $o = $bd->{startChannel} - 1, my $c = 0; $c < $channels; $c++, $o++)
		{
			substr(${$this->{"dataFileMap"}}, $this->{pixelMap}[$o], 1, substr($rgbData, $c, 1));
		}
	};

	# Get font metrics for our string
	my $img2 = new Image::Magick;
	$img2->Set( size=>'1x1' );
	$img2->ReadImage( 'xc:none' );

	$msg =~ s/\\n/\n/g;

	my ($txt_x_ppem, $txt_y_ppem, $txt_ascender, $txt_descender, $txt_width, $txt_height, $txt_max_advance, $txt_predict) =
		$img2->QueryMultilineFontMetrics(text => $msg, font => $font, fill => $fill, pointsize => $size );

	if (0) {
		printf( "x_ppem:      %d\n", $txt_x_ppem);
		printf( "y_ppem:      %d\n", $txt_y_ppem);
		printf( "ascender:    %d\n", $txt_ascender);
		printf( "descender:   %d\n", $txt_descender);
		printf( "width:       %d\n", $txt_width);
		printf( "height:      %d\n", $txt_height);
		printf( "max_advance: %d\n", $txt_max_advance);
		printf( "predict:     %d\n", $txt_predict);
	}

	if ($pos =~ /^[0-9]+,[0-9]+$/) {
		my ($x, $y) = split(',', $pos);
		$createImage->($x, $y);
		$overlayImage->();
	} elsif ($pos eq "scroll") {
		if ($dir eq "R2L") {
			my $extraWidth = $width * 2;
			my $leadIn = $width;
			my $frames = $txt_width + $extraWidth;
			for (my $i = 0; $i < $frames; $i++) {
				$createImage->($leadIn - $i, ($height / 2) - ($txt_ascender / 2));
				$overlayImage->();

				usleep($sleepTime);
			}
		} elsif ($dir eq "L2R") {
			my $extraWidth = $width * 2;
			my $leadIn = $width;
			my $frames = $txt_width + $extraWidth;
			for (my $i = $frames - 1; $i >= 0; $i--) {
				$createImage->($leadIn - $i, ($height / 2) - ($txt_ascender / 2));
				$overlayImage->();

				usleep($sleepTime);
			}
		} elsif ($dir eq "T2B") {
			my $extraHeight = $height * 2;
			my $leadIn = $height;
			my $frames = $txt_height + $extraHeight;
			for (my $i = $frames - 1; $i >= 0; $i--) {
				$createImage->(0, $leadIn - $i);
				$overlayImage->();

				usleep($sleepTime);
			}
		} elsif ($dir eq "B2T") {
			my $extraHeight = $height * 2;
			my $leadIn = $height;
			my $frames = $txt_height + $extraHeight;
			for (my $i = 0; $i < $frames; $i++) {
				$createImage->(0, $leadIn - $i);
				$overlayImage->();

				usleep($sleepTime);
			}
		}
	} elsif ($pos = "center") {
		$createImage->(($width / 2) - ($txt_width / 2) - 1, ($height / 2) - ($txt_ascender / 2) );
		$overlayImage->();
	}

	$this->SetBlockLock($blk, 0);
}

#############################################################################
1;
}
