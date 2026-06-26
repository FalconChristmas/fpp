# FPP SD Image Builders

The `SD/build-image-*.sh` scripts produce flashable FPP SD card images for
Raspberry Pi (32-bit and 64-bit), BeagleBone Black, and PocketBeagle 2 / BB64.
A single command per platform takes a stock Raspbian / rcn-ee base image and
turns it into a release-ready FPP image plus an OS-update `.fppos` squashfs.

For per-platform notes (image source, kernel, gotchas), see:

| Platform | Script | Notes |
|----------|--------|-------|
| Raspberry Pi (32-bit) | `build-image-pi.sh --arch armhf` | [README.RaspberryPi](README.RaspberryPi) |
| Raspberry Pi (64-bit) | `build-image-pi.sh --arch arm64`  | [README.RaspberryPi](README.RaspberryPi) |
| BeagleBone Black      | `build-image-bbb.sh`              | [README.BBB](README.BBB) |
| PocketBeagle 2 / BB64 | `build-image-bb64.sh`             | [README.BB64](README.BB64) |

For installing FPP on an existing system (no image build) — Debian, Ubuntu,
Armbian, x86_64 NUC, etc. — see [README.Debian](README.Debian) /
[README.Armbian](README.Armbian).

## Host requirements

The build scripts run on Linux. **arm64 hosts are strongly preferred**:
arm64+qemu emulating armhf is meaningfully faster than x86_64+qemu, and you
avoid a class of qemu-arm-on-x86 quirks (TLS handshakes, ICMP) that bit
earlier iterations.

Install the build dependencies (Debian / Ubuntu host):

```bash
sudo apt-get install -y \
    qemu-user-static binfmt-support \
    parted dosfstools e2fsprogs \
    zerofree squashfs-tools zip xz-utils \
    wget ca-certificates rsync
```

On arm64 hosts, `qemu-user-static` typically does **not** auto-register the
`qemu-arm` binfmt entry (because the host is already arm64 and only "foreign"
arches get registered). Verify and, if missing, register manually:

```bash
ls /proc/sys/fs/binfmt_misc/qemu-arm   # should print the path
# if missing:
sudo sh -c 'printf ":qemu-arm:M::\x7fELF\x01\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x02\x00\x28\x00:\xff\xff\xff\xff\xff\xff\xff\x00\xff\xff\xff\xff\xff\xff\xff\xff\xfe\xff\xff\xff:/usr/bin/qemu-arm-static:OCF" > /proc/sys/fs/binfmt_misc/register'
```

The build scripts run as root (need `losetup`, `mount`, `chroot`).

## Quick start

```bash
# From the FPP source tree root:
sudo ./SD/build-image-pi.sh --arch arm64 --version 10.0-dev --use-local-src
```

Output lands in `output/`:
- `FPP-v<VERSION>-<PLATFORM>.img.zip` — flashable image (zipped raw)
- `<PLATFORM>-<VERSION>_<OS-DATE>.fppos` — squashfs for in-place OS upgrades

## Common flags (all platforms)

| Flag | Default | Description |
|------|---------|-------------|
| `--version VER` | (required) | FPP version string baked into filenames and `/etc/fpp/rfs_version`. Use `nightly`, `10.0-dev`, `10.0`, etc. |
| `--os-version VER` | `YYYY-MM` (UTC) | OS-version tag embedded in the `.fppos` filename. Match this to the `/etc/fpp/rfs_version` style if you care about consistency. |
| `--branch BRANCH` | `master` | FPP git branch to install. With `--use-local-src`, defaults to your local checkout's current branch. |
| `--use-local-src` | off | rsync your local FPP working tree into `/opt/fpp` inside the image and pass `--skip-clone` to the installer. Lets you iterate on FPP source without committing/pushing. |
| `--fpp-src-dir DIR` | parent of script | Path to your local FPP checkout. |
| `--base-image-url URL` | platform default | Override the base OS image download URL (when upstream rev'd). |
| `--base-image-date DATE` | platform default | Compose the default URL with this build date. On Pi this is the date in the image *filename*. |
| `--base-image-dir-date DATE` | same as `--base-image-date` | Pi only. Date in the download *directory* name, for releases (e.g. 2026-06) where RPi dated the directory differently from the file. |
| `--base-image-sha256 HEX` | known sum for the default Pi image | Optional sha256 to verify the download. Pi ships a built-in default for its stock base image. |
| `--img-size-mb N` | platform default | Output raw image size in MiB. Must be ≥ the decompressed base image size. |
| `--work-dir DIR` | `./build` | Scratch area (decompressed base images, work image). Caches the base image between runs. |
| `--output-dir DIR` | `./output` | Where the final `.img.zip` and `.fppos` land. |
| `--skip-fppos` | off | Skip the squashfs generation. Saves 10–30 min per iteration when you only care about the `.img`. |
| `--skip-zip` | off | Skip the final `zip -9` of the raw `.img`. Saves a few minutes when you're flashing the raw image directly to an SD card for local testing. |
| `--skip-kernel-update` | **on** (Pi), off (BBB) | Skip the kernel update step (`rpi-update` on Pi, FPP-patched kernel `.deb` on BBB). On Pi this is now the default — the 2026-06 Trixie base already ships Linux 6.18 LTS — so the flag is mostly informational there. On BBB, skipping boots whatever kernel the base image shipped with; do NOT use for BBB release builds. |
| `--kernel-update` | — | Pi only. Force the `rpi-update` kernel step back on (the path is retained but disabled by default). Needed if a future RPi base image lags behind the LTS kernel FPP requires. |
| `--keep-work` | off | Don't delete the work image on success (useful for inspecting the output before zipping). |
| `-h`, `--help` | — | Show full per-script help. |

## Platform-specific flag highlights

- **`build-image-pi.sh`** also takes `--arch {armhf|arm64}` (required-ish; default `armhf`).
- **`build-image-bbb.sh`** also takes `--fpp-kernel-ver VER` and `--fpp-kernel-url URL` to override the FPP-patched kernel `.deb` (default pulled from `FalconChristmas/fpp-linux-kernel`).
- **`build-image-pi.sh`** skips the `rpi-update` kernel step by default — the 2026-06 Raspberry Pi OS Trixie base already ships Linux 6.18 LTS. Pass `--kernel-update` to force it back on (e.g. when a future base image lags the required LTS kernel).
- **`build-image-bb64.sh`** has no kernel-update flag — the rcn-ee base image already ships a 6.18 kernel.

## Iteration workflow

When debugging the installer flow itself:

```bash
# Edit SD/FPP_Install.sh in your editor, then:
sudo ./SD/build-image-pi.sh \
    --arch arm64 --version 10.0-dev \
    --use-local-src --skip-fppos --skip-zip --skip-kernel-update --keep-work
```

The flags together get you the fastest possible re-run:
- `--use-local-src` — no git push needed
- `--skip-fppos` — skips the slow xz squashfs
- `--skip-zip` — skips zipping the raw `.img` (flash the `.img` directly)
- `--skip-kernel-update` — skips the kernel download / install (Pi / BBB)
- `--keep-work` — leave the work `.img` in place for inspection

## Performance notes

- **mksquashfs is slow.** A full FPP rootfs (~3-5 GB) compressed with `xz`
  takes 10-30 min depending on host CPU. The build script enables loop-device
  `--direct-io=on` automatically, which roughly doubles I/O throughput on
  NVMe; the rest is just xz being CPU-bound.
- **Fresh chroot per run.** Each invocation decompresses the base image from
  scratch — the cached `.img.xz` stays in `build/` so download is only once,
  but the apt install + FPP compile run fresh every time. ccache reuse
  across runs is not currently wired up.
- **armhf builds are slow under qemu** even on arm64 hosts (typically 1–2 hr).
  Native arm64 builds (Pi64, BB64) finish in ~30 min on a modern arm64 box.

## Output artifacts

```
output/
├── FPP-v10.0-Pi.img            # raw, kept by default after zip
├── FPP-v10.0-Pi.img.zip        # what you flash via Imager
└── Pi-10.0_2026-04.fppos       # OS-update squashfs (if not --skip-fppos)
```

The `.fppos` file is what FPP's web UI offers under "OS Updates" — it
upgrades a running FPP install in place via `upgradeOS-part1.sh`.

## Continuous integration

`.github/workflows/build-images.yml` runs the same scripts on a nightly cron
and on every `v*` tag push. Stable nightly download URLs:

```
https://github.com/FalconChristmas/fpp/releases/download/nightly/FPP-vnightly-Pi.img.zip
https://github.com/FalconChristmas/fpp/releases/download/nightly/FPP-vnightly-Pi64.img.zip
https://github.com/FalconChristmas/fpp/releases/download/nightly/FPP-vnightly-BB64.img.zip
https://github.com/FalconChristmas/fpp/releases/download/nightly/FPP-vnightly-BBB.img.zip
```

(`releases/latest/download/...` continues to point at the most recent
versioned release; nightlies are pre-releases and don't poison that URL.)
