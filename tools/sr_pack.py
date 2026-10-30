#!/usr/bin/env python3
"""Writes the pack (src/port/sr_pack.h) from entry files (sr_data.py, sr_music)."""
import struct
import sys
import zlib


def read_entries(path):
    data = open(path, 'rb').read()
    pos = 0
    while pos < len(data):
        (l,) = struct.unpack_from('<I', data, pos)
        name = data[pos + 4:pos + 4 + l].decode()
        pos += 4 + l
        (size,) = struct.unpack_from('<I', data, pos)
        pos += 4
        yield name, data[pos:pos + size]
        pos += size


def main():
    out = sys.argv[1]
    entries = {}
    for p in sys.argv[2:]:
        for name, b in read_entries(p):
            assert name not in entries, name
            entries[name] = b
    names = sorted(entries, key=lambda n: n.encode())
    count = len(names)
    table_end = 16 + 12 * count
    name_offs = []
    blob = bytearray()
    for n in names:
        name_offs.append(table_end + len(blob))
        blob += n.encode() + b'\0'
    pos = table_end + len(blob)
    data = bytearray()
    data_offs = []
    for n in names:
        pad = (-(pos + len(data))) % 32
        data += b'\0' * pad
        data_offs.append(pos + len(data))
        data += entries[n]
    total = pos + len(data)
    table = b''.join(struct.pack('<III', name_offs[i], data_offs[i], len(entries[n])) for i, n in enumerate(names))
    body = table + bytes(blob) + bytes(data)
    head = struct.pack('<4sII', b'SRD1', count, total)
    crc = zlib.crc32(body) & 0xFFFFFFFF
    with open(out, 'wb') as f:
        f.write(head + struct.pack('<I', crc) + body)
    print('sr_pack: %s, %d entries, %d bytes' % (out, count, total))


if __name__ == '__main__':
    main()
