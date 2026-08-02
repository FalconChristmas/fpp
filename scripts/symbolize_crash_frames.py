#!/usr/bin/env python3
"""Resolve the raw frame addresses in a crash log to function names and lines.

Runs on the device, against the binaries that actually produced the crash.
That matters more than it sounds: FPP builds from source on the device and
supports in-place `git pull && make` upgrades, so two units reporting the same
build string can be running genuinely different binaries -- different Debian
release, different gcc, different code layout. Resolving anywhere else means
guessing which one crashed, and a confidently wrong line number is worse than
no line number at all. Here there is nothing to guess.

Only does anything when fppd fell back from gdb; a successful gdb stack
already carries line numbers. Output is a separate file so that existing
parsers of fppd_crash.log are untouched.

Usage: symbolize_crash_frames.py <crash-log> <output>
"""

import os
import re
import struct
import subprocess
import sys

MAP_RE = re.compile(
    r'^([0-9a-fA-F]+)-([0-9a-fA-F]+)\s+(\S+)\s+([0-9a-fA-F]+)\s+\S+\s+\d+\s+(/.*)$')

ET_EXEC = 2
ADDR2LINE_TIMEOUT = 20


def parse_sections(path):
    """Pull the raw frame list and the module map out of the crash log."""
    frames, maps, section = [], [], None
    try:
        with open(path, 'r', errors='replace') as f:
            for line in f:
                if line.startswith('=== '):
                    if line.startswith('=== raw frames'):
                        section = 'frames'
                    elif line.startswith('=== module map'):
                        section = 'maps'
                    else:
                        section = None
                    continue
                line = line.rstrip('\n')
                if section == 'frames':
                    s = line.strip()
                    if s.startswith('0x'):
                        try:
                            frames.append(int(s, 16))
                        except ValueError:
                            pass
                elif section == 'maps':
                    m = MAP_RE.match(line)
                    if m:
                        maps.append({
                            'start': int(m.group(1), 16),
                            'end': int(m.group(2), 16),
                            'perms': m.group(3),
                            'path': m.group(5),
                        })
    except OSError:
        return [], []
    return frames, maps


def load_bases(maps):
    """Lowest mapped address per file -- the ELF load base for a PIE/DSO."""
    bases = {}
    for m in maps:
        p = m['path']
        if p not in bases or m['start'] < bases[p]:
            bases[p] = m['start']
    return bases


def elf_type(path):
    """ET_EXEC means addresses are absolute and must NOT have a base removed.

    Read from the header rather than parsed out of readelf's text, which is
    locale- and version-dependent.
    """
    try:
        with open(path, 'rb') as f:
            hdr = f.read(20)
        if len(hdr) < 20 or hdr[:4] != b'\x7fELF':
            return None
        endian = '<' if hdr[5] == 1 else '>'
        return struct.unpack(endian + 'H', hdr[16:18])[0]
    except OSError:
        return None


def module_for(addr, maps):
    for m in maps:
        if m['start'] <= addr < m['end'] and 'x' in m['perms']:
            return m
    # Fall back to any mapping of the address; a frame can land in a page whose
    # permissions were changed after load.
    for m in maps:
        if m['start'] <= addr < m['end']:
            return m
    return None


def addr2line(path, offsets):
    """Batch one call per module. No -i: without inline expansion the output is
    exactly two lines per address, which is unambiguous to split. Inline frames
    would be nice but not at the cost of guessing where each address ends."""
    cmd = ['addr2line', '-e', path, '-f', '-C'] + ['0x%x' % o for o in offsets]
    try:
        out = subprocess.run(cmd, capture_output=True, text=True,
                             timeout=ADDR2LINE_TIMEOUT)
    except (OSError, subprocess.SubprocessError):
        return {}
    if out.returncode != 0:
        return {}
    lines = out.stdout.splitlines()
    results = {}
    for i, off in enumerate(offsets):
        fn = lines[2 * i] if 2 * i < len(lines) else '??'
        loc = lines[2 * i + 1] if 2 * i + 1 < len(lines) else '??:0'
        results[off] = (fn.strip(), loc.strip())
    return results


def main():
    if len(sys.argv) != 3:
        sys.stderr.write(__doc__)
        return 2
    crash_log, out_path = sys.argv[1], sys.argv[2]

    frames, maps = parse_sections(crash_log)
    if not frames or not maps:
        # gdb produced a stack, or the log predates the fallback format.
        return 0

    bases = load_bases(maps)
    etypes = {}
    resolved = [None] * len(frames)
    batches = {}

    for i, addr in enumerate(frames):
        m = module_for(addr, maps)
        if not m:
            resolved[i] = (addr, None, None, '??', '??:0')
            continue
        path = m['path']
        if path not in etypes:
            etypes[path] = elf_type(path)
        # Frame 0 is the faulting PC and is exact. Every frame above it is a
        # RETURN address -- the instruction after the call -- so resolving it
        # verbatim reports the line after the call, and for a call in tail
        # position often the following function entirely. Step back one byte.
        lookup = addr if i == 0 else addr - 1
        off = lookup if etypes[path] == ET_EXEC else lookup - bases[path]
        if off < 0:
            resolved[i] = (addr, path, None, '??', '??:0')
            continue
        batches.setdefault(path, []).append((i, off))

    for path, items in batches.items():
        if not os.path.exists(path):
            for i, off in items:
                resolved[i] = (frames[i], path, off, '??', '(binary not present)')
            continue
        got = addr2line(path, [o for _, o in items])
        for i, off in items:
            fn, loc = got.get(off, ('??', '??:0'))
            resolved[i] = (frames[i], path, off, fn, loc)

    known = sum(1 for r in resolved if r and r[3] not in ('??', ''))
    try:
        with open(out_path, 'w') as f:
            f.write('=== addr2line symbolization (resolved on-device) ===\n')
            f.write('%d of %d frames resolved to a function.\n' % (known, len(frames)))
            f.write('Frames above #00 are return addresses; the lookup steps back one\n'
                    'byte so the line refers to the call, not the instruction after it.\n\n')
            for i, r in enumerate(resolved):
                if r is None:
                    continue
                addr, path, off, fn, loc = r
                where = '%s+0x%x' % (os.path.basename(path), off) if path and off is not None \
                    else (os.path.basename(path) if path else '<unmapped>')
                f.write('#%02d 0x%012x  %-28s %s\n' % (i, addr, where, fn))
                if loc and loc not in ('??:0', '??'):
                    f.write('        at %s\n' % loc)
    except OSError:
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
