# Testing

FPP's tests live in [FalconChristmas/fpp-tests](https://github.com/FalconChristmas/fpp-tests),
in two suites:

- **Runtime scenarios** (`scenarios/`). They start a real `fppd`, drive it through its HTTP
  API, and check what it does. This is where tests for fppd itself belong: playlists, the
  scheduler, commands and presets, channel outputs, overlays, MultiSync, the bridge, and
  startup and shutdown.
- **Web UI** (`web/`). Playwright tests for the pages under `www/`, run against a commit's
  UI served from Docker the way a player serves it. See [Web UI tests](#web-ui-tests).

The tests live in their own repository because every FPP device has a full clone of
this one. Tests, fixtures and the CI harness add nothing at runtime, so they stay out of the
history that every device downloads.

## What a scenario checks

Each scenario starts a fresh `fppd` on an empty media directory, writes the config it needs
(playlists, presets, output configs), and drives `fppd` through its own HTTP server on port
32322. A scenario passes only if all of the following hold:

- every request was answered (a request that times out means `fppd` is wedged),
- `fppd` is still running,
- it exits 0 on SIGTERM, and
- the sanitizer reported nothing (in an ASan or TSan build).

So every scenario catches deadlocks, crashes and memory errors along the way, as well as the
behavior it asserts. When one fails, its media directory is kept with `fppd.log`, any
sanitizer report, and a gdb backtrace of every thread if `fppd` was still running. For a
wedge, read the backtrace first.

## Running it

Check fpp-tests out next to your fpp checkout (`../fpp-tests`).

**In Docker (macOS, Linux, or Windows with WSL2).** This builds any fpp commit with ASan and
runs the suite against it:

```sh
cd ../fpp-tests
./run.sh                                   # ../fpp at HEAD
./run.sh --ref 7e171ae48^ -k presets -v    # any commit, any pytest selection
./run.sh --flavor tsan                     # asan (default), tsan, debug or optimized
```

`run.sh` builds from a commit, so uncommitted changes aren't tested. Commit locally first.

**On an FPP device, against `/opt/fpp` as currently built.** Uncommitted changes are included,
and nothing is exported or rebuilt:

```sh
cd /opt/fpp/src && make            # or make debug / make tsan
cd ~/fpp-tests && sudo ./run-local.sh
```

The device's own `fppd` keeps running. Each test `fppd` runs in private mount and network
namespaces: a loopback-only network, its own `/tmp`, `/run` and `/dev/shm`, an overlay over the
tree so nothing it writes there reaches the real one, and no sound or SPI devices. Don't run
it during a show, though: a test `fppd` still competes with the show for CPU. ASan doesn't run
on the Pi kernels (see [DeveloperNotes.md](./DeveloperNotes.md)), so use a normal, debug, tsan
or ubsan build there.

**On GitHub.** The suite runs nightly against `master`, on every pull request to fpp-tests,
and on demand from that repository's Actions tab, for any fpp branch or commit. You can write
a scenario without Docker or a device: fork fpp-tests, add it, and open a pull request.

## Adding a scenario with a fix

**Every fix to fppd's runtime behavior should come with a scenario that fails without it.**
A test that passes on both sides of a fix proves nothing. `scripts/verify-fix.sh` in fpp-tests
checks both sides:

```sh
scripts/verify-fix.sh <fix-commit> scenarios/test_command_presets.py -k reload
```

It runs the scenario on the fix's parent and expects a failure, then on the fix and expects a
pass. If an earlier change already hides the bug at the parent (a general guard that landed
first), pass `--base` with a commit from before both.

Name the fix commit in the scenario's docstring, and describe what went wrong before it. A
minimal scenario looks like this:

```python
def test_edit_reloads_without_wedging(fppd):
    """Saving commandPresets.json while fppd runs reloads it in place.

    Regression: 7e171ae48. The reload took a mutex its caller already held,
    and the first edit after startup deadlocked the main loop.
    """
    fppd.write("config/commandPresets.json", {"commands": [set_variable("X", 1)]})
    fppd.start()
    ...
```

The fixture provides `write` and `settings` (set up the media directory), `start`,
`get`/`post`/`command` (each fails with "fppd is wedged" on a timeout), `wait_for` and `log`.
`udp.Listener` records what `fppd` sends to a local port, for checking channel outputs.

Some fixes need extra care:

- **Races.** Make the scenario fail every time, not now and then: repeat the triggering
  action in a loop until the failure is near-certain. A test that is red one run in ten is
  not a regression test yet.
- **Hardware outputs.** PRU, DPI, panels and serial outputs can't run in Docker. Write the
  scenario anyway and run it with `run-local.sh` on a device that has the hardware, and say
  so in the docstring.
- **Pure logic** (date math, parsers, bounds checks) can still be driven end to end through
  the config and API that reach it. That is usually only a few lines.

## Web UI tests

`run-web.sh` in fpp-tests builds a commit the way `run.sh` does, serves its UI from Docker
(Apache, php-fpm, the commit's own site config and a freshly generated CSP, with that build's
`fppd` behind it), and runs the Playwright suite in `web/` against it from your machine. It
needs Docker and Node.js 20+.

```sh
cd ../fpp-tests
./run-web.sh                               # ../fpp at HEAD
./run-web.sh --ref <commit> tests/fpp.spec.ts -g Playlists
./run-web.sh --serve                       # serve the UI on 127.0.0.1:8080 to write a test against
```

The `fppd` behind the pages is checked as in a scenario: the run fails if it crashed, wedged
or reported a sanitizer error. A UI fix gets a test that fails without it, proved the same way
as a scenario:

```sh
scripts/verify-fix.sh <fix-commit> web/tests/<spec>.spec.ts
```

The suite runs nightly against `master` alongside the scenarios.

## Old commits that don't build

`fixups/*.patch` in fpp-tests are build-only backports, each named after the fpp commit it
comes from. A fixup is applied to an exported tree only when it applies cleanly, so an old
commit can still be built for a red-before check. Add one when a historical commit doesn't
compile under the sanitizer flags.
