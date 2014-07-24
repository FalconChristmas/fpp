The jQuery-Upload-File plugin is available via github at the following link:

    https://github.com/hayageek/jquery-upload-file/

The stock upload.php file has been copied to fpp/www/jqupload.php and modified
for our use, to move uploaded files into their proper directories, etc..

Files not needed for FPP such as the demo and sample json have been removed
from the jQuery-Upload-File download and not imported into FPP git.

=============================================================================

Below is a list of any modifications done to the original source:

- Changed value of 'fileCounterStyle' setting from "). " to ") " since the
  dot seems out of place and could be mis-interpreted to be part of
  the filename.
