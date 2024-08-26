# Valgrind

The version of Valgrind that is installable via apt-get is way too old (over 8 years old)
and will not really work for FPP on arm.   Many false positives occur, bunch of unhandled syscalls,
etc...   Building a more recent version of Valgrind from source is required:

Grab the current release from https://valgrind.org/downloads/current.html
Extract to a directory in /opt, run "./configure" and then "make ; make install"

# ccache

For the release branches, tar.gz's of the /root/.ccache directories are uploaded to
https://github.com/FalconChristmas/FalconChristmas.github.io/tree/master/ccache
so that when a user hits the "Upgrade" button, it will download the tar.gz, unpack it,
and then the build proceeds fairly quickly.  (assuming internet access)

For developer and master branch that changes often, updating the release ccaches would
be time consuming.  Instead, if ccache 4.6+ is available, there is a "secondary-storage"
option that will allow ccache to grab artifacts from a shared ccache on http://kulplights.com/ccache
To Enable, run:
ccache --set-config secondary_storage='http://kulplights.com/ccache|layout=flat|read-only=true'

That will provide read-only access to the Developer ccache.   For trusted developers that
have the required authorization tokens, the ccache can be read/write to share their
build artifacts with others:

ccache --set-config secondary_storage='http://USERNAME:PASSWORD@kulplights.com/ccache-upload|layout=flat|read-only=false'
ccache --set-config reshare=true

Note: there is an /opt/fpp/SD/buildCCACHE.sh script that can be run to build the required
version of ccache.  Debian currently does not include a recent enough version of ccache
and thus is must be build from source.

# distcc

On the slow, single core, armv7 SBC's such as the BeagleBone's, it can take a long time to
fully rebuild FPP if a change is made that changes a common header.  Using distcc to
have the compile part run on a faster Pi4 can dramatically speed things up.   A full
rebuild can drop from 45m to about 15m.  To set this up, you need to install distcc
on both the Pi and Beagle:

```
sudo apt-get install distcc
```

On the Pi, you need to run something similar to:
sudo distccd -a 192.168.0.0/16 -j 6 --daemon
but use the network/mask that works for you network.   If you want it setup to start
automatically with all the proper settings, edit /etc/default/distcc and set:

```
STARTDISTCC="true"
ALLOWEDNETS="192.168.0.0/16"
```

and comment out the listener line.   Then run "systemctl enable distcc".

On the Beagle, before running make, run:

```export
export DISTCC_HOSTS="192.168.3.71"
```

but use the IP for you pi.   If you have multiple Pi's, you can add them all to the
HOSTS by space separating them.   Then run "make -j4" or similar to have it compile
multiple files in parallel. The Beagle will do the precompile step, send it to the
Pi to be compiled, and then the Beagle will do all the linking.

# zero-md

zero-md is installed in the fpp ww structure to allow quick rendering of markdown (.md) files in the UI html
To display a .md include the following in the generated page HTML:

```html
<zero-md src="file.md"></zero-md>
```

