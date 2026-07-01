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

# nocc (recommended)

nocc is a distributed compiler like distcc, but it offloads **preprocessing as well as
compilation** to the helper. That one difference is decisive on a slow, single-core
board: under distcc the client still preprocesses every file locally, and on a
BeagleBone that preprocessing -- not the compile -- is the bottleneck, so the fast
helper sits mostly idle waiting on the client. nocc does only a light include-scan on
the client and preprocesses **and** compiles on the helper; just the final link runs
locally. A from-scratch BeagleBone build measured **~5.5 min with nocc vs ~16 min with
distcc**. **Prefer nocc, especially on the single-core armv7 boards.**

Under the hood a small `nocc` wrapper talks to a persistent per-machine `nocc-daemon`,
which streams work over gRPC to an `nocc-server` running on the helper.

nocc isn't in Debian, so FPP serves it from its own signed apt repository
(`https://falconchristmas.github.io/fpp-apt`). Recent FPP images already include it;
otherwise `apt-get install nocc` once that repo is configured (the FPP installer does
this automatically).

### What FPP does automatically when nocc is selected

With `NOCC_SERVERS` set (the web UI and build scripts export it), the makefiles:

* use **`nocc` as the compiler launcher directly** -- deliberately *not* `ccache nocc`.
  ccache in front would run the preprocessor locally just to build its cache key --
  exactly the local work nocc exists to eliminate -- and a distributed NOPCH build
  never matches the PCH-built shared ccache anyway. nocc keeps its own compiled-object
  cache on the helper, so caching still happens, just server-side.
* **disable the precompiled header**, and
* invoke the **target-triplet** compiler so an aarch64 helper cross-compiles for a
  32-bit board.

The last two -- plus "the helper needs the matching cross-compiler", "the compiler
major versions must match", and "every `.cpp` must directly `#include` what it uses" --
are all identical to distcc (see that section below for the details).

## Setting up the helper

On the fast machine that will do the compiling:

```
sudo /opt/fpp/scripts/setup_nocc_host.sh
```

This installs the aarch64 + armhf toolchains and enables the `nocc-server` service on
TCP **43210**. nocc-server has **no access control of its own**, so keep the port
LAN-only: pass `NOCC_FIREWALL=1` to add an nftables rule that allows 43210 only from
your LAN (`sudo NOCC_FIREWALL=1 ./setup_nocc_host.sh 192.168.7.0/24` to force the
CIDR). Override the port with `PORT=...`. Turn the helper back off with
`sudo /opt/fpp/scripts/setup_nocc_host.sh disable`.

(nocc has no mDNS auto-discovery -- list the helper explicitly.)

## Using it from the web UI

On the client, go to **Status/Control > FPP Settings > Developer** (UI level 3), set
**Distributed Compile** to **nocc**, and set **Compile Hosts** to your helper. A
**Rebuild FPP** from the Developer tab, or a branch change, then builds through nocc.

## Using it from the command line

On the client, export `NOCC_SERVERS` and build. **No compiler flags are needed** -- the
makefiles select the triplet compiler and default `NOCC_GO_EXECUTABLE` automatically:

```
export NOCC_SERVERS="fpppi5:43210"     # host:PORT -- the port is REQUIRED (default 43210)
cd /opt/fpp/src
make -j6
```

Unlike distcc's space-separated `host[:port]`, `NOCC_SERVERS` **must include the port**
and is **`;`-separated** for several helpers (`hostA:43210;hostB:43210`). Set
`NOCC_LOG_FILENAME=/tmp/nocc.log` to capture nocc's log; it prints `num servers 1` when
it's offloading and "compiling locally" if it ever falls back.

> **-j sizing.** Because nocc offloads preprocessing too, the only local work is the
> link, so `-j` can go much higher than with distcc. With the **mold** linker (FPP's
> default on these boards, and memory-light) even a ~480MB BeagleBone runs `-j6`
> comfortably; without mold the links are heavier, so stay nearer `nCPU+2`. The
> web-UI / **Rebuild** path computes an appropriate `-j` for you.

# distcc

> **nocc (above) is usually the better choice**, especially on single-core boards:
> distcc preprocesses locally, and on those boards that local preprocessing is itself
> the bottleneck. Reach for distcc when a helper can't run nocc-server, or you already
> have a distcc helper in place.

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
LAN (auto-detected; override with a CIDR argument or `ALLOWEDNETS="..."`), advertises the
helper via Zeroconf/mDNS so clients can auto-discover it, and starts the daemon. mDNS is on
by default; pass `MDNS=0` to disable the advertising. Turn the helper back off with
`sudo /opt/fpp/scripts/setup_distcc_host.sh disable`.

> The stock Debian distccd systemd unit mishandles multiple networks: it passes them all to
> a single `--allow`, which only honors the *first* one (so other clients get
> "denied by access list"). The setup script installs a drop-in that emits one `--allow`
> per network. If you configure distccd by hand, give each network its own `--allow`, e.g.
> `distccd --allow 127.0.0.1 --allow 192.168.2.0/24 --daemon`.

## Using it from the web UI

On the client, go to **Status/Control > FPP Settings > Developer** (UI level 3) and set
**Distributed Compile** to either:

* **distcc** and set **Compile Hosts** to your helper, e.g. `fpppi5:3632/6,lzo` (each
  entry is `host[:port][/jobs][,options]`; space-separate several), or
* **distcc + zeroconf** to auto-discover helpers via mDNS (the helper advertises itself
  by default, so nothing extra is needed on it -- **Compile Hosts** is ignored).

> **mDNS discovery works across architectures.** FPP browses the advertised `_distcc._tcp`
> services with avahi and feeds them as *explicit* hosts, so a 64-bit Pi helper can serve a
> 32-bit BeagleBone. (It deliberately avoids distcc's built-in `+zeroconf`, which filters
> helpers by CPU architecture and would silently ignore a cross-arch helper.) The helper
> just needs to be advertising, which `setup_distcc_host.sh` does by default.

A **Rebuild FPP** from the Developer tab, or a branch change, then builds through distcc.

## Using it from the command line

On the client, export `DISTCC_HOSTS` and build. **No compiler flags are needed** -- the
makefiles select the right triplet compiler automatically:

```
export DISTCC_HOSTS="192.168.3.71"     # your helper's IP/host; space-separate several
cd /opt/fpp/src
make -j3                               # see the note on -j below
```

Use the IP/hostname of your helper; to use several, space-separate them (optionally with
`/jobs` and `,lzo` per host). Watch the live distribution with
`DISTCC_VERBOSE=0 distccmon-text 1`.

Unlike nocc, distcc **preprocesses every file on the client**, so on a single-core board
that local preprocessing -- not the helper -- caps throughput: `-j3` (≈ local cores + 2)
is about the practical maximum there; going higher just piles up local preprocessing and
risks swap. On a multi-core client you can raise it toward the helper's total job count.
(The web-UI / **Rebuild** path picks this automatically.)

> On a memory-tight board, be conservative with `-jN`: if the helper becomes unreachable
> mid-build, distcc falls back to compiling **locally**, and too many parallel local
> compiles can exhaust RAM and thrash the board.

# zero-md

zero-md is installed in the fpp ww structure to allow quick rendering of markdown (.md) files in the UI html
To display a .md include the following in the generated page HTML:

```html
<zero-md src="file.md"></zero-md>
```

