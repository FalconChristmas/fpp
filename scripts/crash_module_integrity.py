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

An ordinary read cannot tell RAM damage from disk damage. A mapped library's
code pages ARE its page cache pages, so a bit flipped in RAM shows up in
open().read() too: the code at pc "matches the file" and the md5 mismatches the
package, which reads as a library damaged on disk. So wherever the answer turns
on it -- the code at pc, a file whose md5 mismatches its package, and the
module the pc is in -- the file is read again with O_DIRECT, which bypasses
the page cache and returns what the storage holds. The page cache disagreeing
with the storage is memory damage (or a write that never reached the disk),
not a damaged file. O_DIRECT also flushes dirty pages first, so a write still
waiting for writeback does not look like damage.

Usage: crash_module_integrity.py <fppd_crash_context.log> <output.json>

Writes nothing when the context log has no module map (macOS, old builds).
"""

import errno
import glob
import hashlib
import json
import mmap
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


# O_DIRECT needs the buffer, file offset and length aligned to the device's
# logical block size; a page covers both 512 byte and 4K devices.
DIRECT_ALIGN = 4096
DIRECT_CHUNK = 1 << 20


def direct_chunks(path, offset=0, length=None):
    """The file's bytes as the storage holds them, bypassing the page cache.

    Raises OSError where O_DIRECT is not supported (tmpfs, some overlay
    setups, macOS)."""
    flag = getattr(os, 'O_DIRECT', 0)
    if not flag:
        raise OSError(errno.EINVAL, 'O_DIRECT not available')
    fd = os.open(path, os.O_RDONLY | flag)
    try:
        buf = mmap.mmap(-1, DIRECT_CHUNK)  # anonymous, so page aligned
        pos = offset - offset % DIRECT_ALIGN
        skip = offset - pos
        while length is None or length > 0:
            n = os.preadv(fd, [buf], pos)
            if n <= skip:
                break
            data = buf[skip:n]
            skip = 0
            if length is not None:
                data = data[:length]
                length -= len(data)
            yield data
            if n < DIRECT_CHUNK:
                break  # end of file
            pos += n
    finally:
        os.close(fd)


def read_direct(path, offset, length):
    return b''.join(direct_chunks(path, offset, length))


def hash_file_direct(path):
    md5, sha = hashlib.md5(), hashlib.sha256()
    for chunk in direct_chunks(path):
        md5.update(chunk)
        sha.update(chunk)
    return md5.hexdigest(), sha.hexdigest()


def module_for(maps, addr, length):
    return next((m for m in maps if m['start'] <= addr and addr + length <= m['end']), None)


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
        m = module_for(maps, addr, len(mem))
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
        if disk != mem:
            entry['result'] = 'DIFFERS from file'
            out.append(entry)
            continue
        # disk above came through the page cache, which shares its pages with
        # the mapping; only the storage itself can disagree with a flipped bit
        try:
            stored = read_direct(path, offset, len(mem))
        except OSError as e:
            entry['storage'] = 'unreadable: %s' % e.strerror
            entry['result'] = 'matches file'
            out.append(entry)
            continue
        if stored == mem:
            entry['storage'] = 'matches'
            entry['result'] = 'matches file'
        else:
            entry['storage'] = stored.hex()
            entry['result'] = 'page cache DIFFERS from storage'
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
    # an unpackaged module has nothing to compare against but its own storage
    pc_modules = {m['path'] for m in (module_for(maps, a, len(c)) for a, c in code) if m}

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
        if entry['status'] != 'ok' and (pkg or p in pc_modules):
            try:
                smd5, ssha = hash_file_direct(r)
            except OSError as e:
                entry['storage'] = 'unreadable: %s' % e.strerror
            else:
                if (smd5, ssha) == (md5, sha):
                    entry['storage'] = 'matches page cache'
                else:
                    entry['storage_md5'] = smd5
                    if not pkg:
                        entry['storage_sha256'] = ssha
                    entry['status'] = 'page cache DIFFERS from storage'
        modules.append(entry)
    for e in modules:
        counts[e['status']] = counts.get(e['status'], 0) + 1

    report = {
        'summary': counts,
        'differs_from_package': [e['path'] for e in modules if e['status'] == 'DIFFERS from package'],
        'page_cache_differs': [e['path'] for e in modules if e['status'] == 'page cache DIFFERS from storage'],
        'code_at_pc': check_code(maps, code),
        'modules': modules,
    }
    with open(sys.argv[2], 'w') as f:
        json.dump(report, f, indent=2)
    return 0


if __name__ == '__main__':
    sys.exit(main())
