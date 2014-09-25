
The MultiSync Video support required minor changes to the omxplayer binary.
These changes include:

- Fixing a speed-adjustment bug
- Adding new commands for fine-grained speed control necessary for syncing
  video running on multiple Pi units across a network.
- Modifying the status output frequency to occur more often to allow for
  tighter sync between multiple Pi's.

This directory includes the patch necessary to compile this modified
omxplayer.bin binary.

The source code to omxplayer itself is locate at:

  https://github.com/popcornmix/omxplayer.git

The prepare-native-raspbian.sh from the omxplayer source repository was used
to build omxplayer initially, then the patch is applied and a new omxplayer.bin
is compiled and copied to /usr/bin/omxplayer.bin on the FPP SD image.

