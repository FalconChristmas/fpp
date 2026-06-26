# ASan (Address Sanitizer) and TSan (Thread Sanitizer)

FPP can be built with [ASan](https://github.com/google/sanitizers/wiki/addresssanitizer)/[TSan](https://github.com/google/sanitizers/wiki/threadsanitizercppmanual)
support, but as these are sanitizer modes, they are **not recommended** for production. Building in
this manner will consume significantly more memory and is only recommended for 64-bit platforms with
significant RAM access, such as Docker on Linux.

To build with these modes:
`make asan`

or

`make tsan`

Highly recommended to perform a `make clean` before building the first time.

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
ccache --set-config remote_storage='http://kulplights.com/ccache|layout=flat|read-only=true'

That will provide read-only access to the Developer ccache.   For trusted developers that
have the required authorization tokens, the ccache can be read/write to share their
build artifacts with others:

ccache --set-config remote_storage='http://USERNAME:PASSWORD@kulplights.com/ccache-upload|layout=flat|read-only=false'
ccache --set-config reshare=true

Note: there is an /opt/fpp/SD/buildCCACHE.sh script that can be run to build the required
version of ccache.  Debian currently does not include a recent enough version of ccache
and thus is must be build from source.

# distcc

On the slow, single-core, armv7 SBCs such as the BeagleBones, a full rebuild of FPP can
take a very long time -- especially when a common header changes and the ccache can no
longer help. distcc offloads the actual compilation to a faster machine (a Pi 5, a
desktop, ...), which can cut a from-scratch BeagleBone rebuild from well over an hour down
to a fraction of that. The client preprocesses each file locally, ships it to a helper to
compile, and does the final link locally.

### What FPP does automatically when `DISTCC_HOSTS` is set

The makefiles detect `DISTCC_HOSTS` and, without any further flags:

* use `ccache distcc` instead of plain `ccache`,
* **disable the precompiled header** (distcc and the PCH don't mix), and
* invoke the **target-triplet** compiler (e.g. `arm-linux-gnueabihf-g++`) instead of plain
  `g++`. distcc runs whatever compiler *name* it's given on the helper, so the triplet name
  is what lets a 64-bit aarch64 helper cross-compile for a 32-bit armhf BeagleBone.

Two things to know:

1. **The helper needs the matching cross-compiler** for every client ABI it serves -- the
   `arm-linux-gnueabihf` toolchain for 32-bit boards and the native `aarch64-linux-gnu`
   compiler for 64-bit boards. `setup_distcc_host.sh` installs both. (Only the
   cross-*compiler* is needed -- not a target sysroot -- because preprocessing and linking
   happen on the client.)
2. **The compiler major version must match** between client and helper (e.g. both GCC 14).
   The simplest way to guarantee that is to run the same FPP OS image everywhere.

Because the PCH is disabled under distcc, every `.cpp` must directly `#include` the headers
it uses. A file that fails *only* under distcc with errors like `'WarningHolder' has not
been declared` is missing a direct include -- add it to that source file.

## Setting up the helper

On the fast machine that will do the compiling:

```
sudo /opt/fpp/scripts/setup_distcc_host.sh
```

This installs distcc plus the aarch64 and armhf GCC toolchains, restricts access to your
LAN (auto-detected; override with a CIDR argument or `ALLOWEDNETS="..."`), and starts the
daemon. Add `MDNS=1` to also advertise the helper via Zeroconf/mDNS. Turn it back off with
`sudo /opt/fpp/scripts/setup_distcc_host.sh disable`.

> The stock Debian distccd systemd unit mishandles multiple networks: it passes them all to
> a single `--allow`, which only honors the *first* one (so other clients get
> "denied by access list"). The setup script installs a drop-in that emits one `--allow`
> per network. If you configure distccd by hand, give each network its own `--allow`, e.g.
> `distccd --allow 127.0.0.1 --allow 192.168.2.0/24 --daemon`.

## Using it from the web UI

On the client, go to **Status/Control > FPP Settings > Developer** (UI level 3) and either:

* enable **Use distcc for Compiles** and set **Distcc Hosts** to your helper, e.g.
  `fpppi5:3632/6,lzo` (each entry is `host[:port][/jobs][,options]`; space-separate several), or
* enable **Use distcc for Compiles** and check **Discover distcc Hosts via mDNS** (and run
  the helper setup with `MDNS=1`).

> **mDNS discovery works across architectures.** FPP browses the advertised `_distcc._tcp`
> services with avahi and feeds them as *explicit* hosts, so a 64-bit Pi helper can serve a
> 32-bit BeagleBone. (It deliberately avoids distcc's built-in `+zeroconf`, which filters
> helpers by CPU architecture and would silently ignore a cross-arch helper.) The helper
> just needs to be advertising — run its setup with `MDNS=1`.

A **Rebuild FPP** from the Developer tab, or a branch change, then builds through distcc.

## Using it from the command line

On the client, export `DISTCC_HOSTS` and build. **No compiler flags are needed** -- the
makefiles select the right triplet compiler automatically:

```
export DISTCC_HOSTS="192.168.3.71"     # your helper's IP/host; space-separate several
cd /opt/fpp/src
make -j6                               # a bit above the helper's total job slots
```

Use the IP/hostname of your helper; to use several, space-separate them (optionally with
`/jobs` and `,lzo` per host). `-jN` should be a little higher than the helper's total job
count so the client keeps it fed while it preprocesses locally. Watch the live distribution
with `DISTCC_VERBOSE=0 distccmon-text 1`.

> On a memory-tight board, be conservative with `-jN`: if the helper becomes unreachable
> mid-build, distcc falls back to compiling **locally**, and too many parallel local
> compiles can exhaust RAM and thrash the board.

# zero-md

zero-md is installed in the fpp ww structure to allow quick rendering of markdown (.md) files in the UI html
To display a .md include the following in the generated page HTML:

```html
<zero-md src="file.md"></zero-md>
```

