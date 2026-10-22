#!/usr/bin/env python3
"""Check the files fppd had mapped at a crash against what they should contain.

A crash in a library that was damaged on disk, or replaced under the running
process, looks like a code bug and is not one: the stack points at a function
that is fine in every other copy of that library. This says which case a
report is, from three checks:

  * every file in the crash's module map, against the md5 dpkg recorded when
    its package was installed (/var/lib/dpkg/info/*.md5sums). A mismatch is a
    file that is not what its package shipped.
  * files no package owns -- fppd and libfpp, plugin .so files -- get a sha256,
    so a report can be matched to a known build, closed-source plugins included.
  * the instruction bytes fppd saved from memory at the faulting pc, against
    the same bytes in the file on disk. Different means the file changed after
    fppd loaded it; equal but the file mismatching its package means it was
    already damaged when it was loaded.

Usage: crash_module_integrity.py <fppd_crash_context.log> <output.json>

Writes nothing when the context log has no module map (macOS, old builds).
"""

import glob
import hashlib
import json
import os
import re
import sys

MAP_RE = re.compile(
    r'^([0-9a-fA-F]+)-([0-9a-fA-F]+)\s+(\S+)\s+([0-9a-fA-F]+)\s+\S+\s+\d+\s+(/.*)$')
CODE_RE = re.compile(r'^0x([0-9a-fA-F]+):((?: [0-9a-f]{2})+)\s*$')
DELETED = ' (deleted)'
DPKG_INFO = os.environ.get('FPP_DPKG_INFO_DIR', '/var/lib/dpkg/info')


def parse_context(path):
    maps, code = [], []
    section = None
    with open(path, 'r', errors='replace') as f:
        for line in f:
            if line.startswith('=== '):
                if line.startswith('=== module map'):
                    section = 'maps'
                elif line.startswith('=== code at pc'):
                    section = 'code'
                else:
                    section = None
                continue
            line = line.rstrip('\n')
            if section == 'maps':
                m = MAP_RE.match(line)
                if m:
                    maps.append({
                        'start': int(m.group(1), 16),
                        'end': int(m.group(2), 16),
                        'offset': int(m.group(4), 16),
                        'path': m.group(5),
                    })
            elif section == 'code':
                m = CODE_RE.match(line)
                if m:
                    code.append((int(m.group(1), 16), bytes.fromhex(m.group(2).replace(' ', ''))))
    return maps, code


def hash_file(path):
    md5, sha = hashlib.md5(), hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(1 << 20), b''):
            md5.update(chunk)
            sha.update(chunk)
    return md5.hexdigest(), sha.hexdigest()


def dpkg_md5s(wanted):
    """{path without leading /: (md5, package)} for the paths in wanted."""
    found = {}
    for md5sums in glob.glob(os.path.join(DPKG_INFO, '*.md5sums')):
        pkg = os.path.basename(md5sums)[:-len('.md5sums')]
        try:
            with open(md5sums, 'r', errors='replace') as f:
                for line in f:
                    parts = line.rstrip('\n').split(None, 1)
                    if len(parts) == 2 and parts[1] in wanted:
                        found[parts[1]] = (parts[0], pkg)
        except OSError:
            pass
    return found


def check_code(maps, code):
    """Each saved code range against the file it was mapped from."""
    out = []
    for addr, mem in code:
        entry = {'address': hex(addr), 'memory': mem.hex()}
        m = next((m for m in maps if m['start'] <= addr and addr + len(mem) <= m['end']), None)
        if not m:
            entry['result'] = 'not in a file mapping'
            out.append(entry)
            continue
        path = m['path']
        offset = addr - m['start'] + m['offset']
        entry.update({'module': path, 'file_offset': hex(offset)})
        if path.endswith(DELETED):
            entry['result'] = 'file deleted after it was loaded'
            out.append(entry)
            continue
        try:
            with open(path, 'rb') as f:
                f.seek(offset)
                disk = f.read(len(mem))
        except OSError as e:
            entry['result'] = 'unreadable: %s' % e.strerror
            out.append(entry)
            continue
        entry['file'] = disk.hex()
        entry['result'] = 'matches file' if disk == mem else 'DIFFERS from file'
        out.append(entry)
    return out


def main():
    if len(sys.argv) != 3:
        print('Usage: crash_module_integrity.py <fppd_crash_context.log> <output.json>', file=sys.stderr)
        return 2
    try:
        maps, code = parse_context(sys.argv[1])
    except OSError:
        return 0
    if not maps:
        return 0

    paths = sorted({m['path'] for m in maps})
    modules = []
    real = {}
    for p in paths:
        if p.endswith(DELETED):
            modules.append({'path': p, 'status': 'deleted after load'})
            continue
        # The map names a library by the path it was opened under; on a
        # merged-/usr system dpkg records /lib/... as usr/lib/...
        real[p] = os.path.realpath(p)
    owned = dpkg_md5s({r.lstrip('/') for r in real.values()} | {p.lstrip('/') for p in real})

    counts = {}
    for p, r in real.items():
        entry = {'path': p}
        if r != p:
            entry['realpath'] = r
        try:
            md5, sha = hash_file(r)
        except OSError as e:
            entry['status'] = 'unreadable: %s' % e.strerror
            modules.append(entry)
            continue
        pkg = owned.get(r.lstrip('/')) or owned.get(p.lstrip('/'))
        if pkg:
            entry['package'] = pkg[1]
            entry['status'] = 'ok' if pkg[0] == md5 else 'DIFFERS from package'
            if entry['status'] != 'ok':
                entry['md5'] = md5
                entry['package_md5'] = pkg[0]
        else:
            entry['status'] = 'unpackaged'
            entry['sha256'] = sha
        modules.append(entry)
    for e in modules:
        counts[e['status']] = counts.get(e['status'], 0) + 1

    report = {
        'summary': counts,
        'differs_from_package': [e['path'] for e in modules if e['status'] == 'DIFFERS from package'],
        'code_at_pc': check_code(maps, code),
        'modules': modules,
    }
    with open(sys.argv[2], 'w') as f:
        json.dump(report, f, indent=2)
    return 0


if __name__ == '__main__':
    sys.exit(main())
