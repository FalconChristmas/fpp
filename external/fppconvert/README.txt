xLights is covered by the GPL v3, so the fppconvert binary is also
covered by GPL v3 while the rest of the Falcon Player is covered under
GPL v2.
=========================================================================

PreRequesites:
- wxWidgets v3.0.1 must be already built on the system you are
  trying to compile fppconvert on.  Instructions for doing this
  are in the xLights' README.linux file.

=========================================================================

Instructions for building xLights' sequence conversion code as a
stand-alone executable called 'fppconvert'.

- Clone a copy of the latest xLights source into 'xLights' subdir

  git clone https://github.com/smeighan/xLights.git xLights

- Make fppconvert

  NOTE: If you did not run "make install" for wxWidgets you can still build
  by including the build location in your $PATH during make.  This will
  allow the build to find "wx-config" which will generate the proper include
  paths and link paths for the fppconvert binary to build and link.

  make

- Test

  ./fppconvert SOME_SEQUENCE_INPUT_FILE OUTPUT_FILE_FORMAT

  for example:

  ./fppconvert We_Wish_You_A_Merry_Christmas.xseq Falcon

  The list of OUTPUT file formats can be found by looking at the
  DoConversion() function in xLights_TabConvert.cpp.  This function
  looks at the first 3 characters of the format name only.

  Since fppconvert uses xLights' DoConversion() function, we can't
  control the output filename.  In fppconvert.cpp is an example of
  how to do the conversion ourself to be able to control the output
  filename, but we would need to reproduce the logic from DoConversion()
  to detect which sequence read and write functions to use based on
  the user's input filename and output filename or format and filename.

=========================================================================

More nitty gritty details.

A helper shell script convert_TabConvert.sh is used to take xLights'
TabConvert.cpp and convert it to a set of standalone compilable
xLights_TabConvert.cpp and xLights_TabConvert.h files.  This shell
script removes some GUI related items and hard-codes a few things such
as the "blank at end of sequence" value (to false in this case).
The code is converted from a Class to standalone functions in the
process.  The member variables of the class are defined at the top of
fppconvert.cpp to allow the functions we want to be used stand-alone.

A helper Perl script strip_function.pl is used by convert_TabConvert.sh
to strip out a few functions that we don't want for now.
