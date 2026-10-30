#!/usr/bin/env python3
"""
Second Reality data -> Game & Watch.

The demo's data (pictures, fonts, tables, the music) are C++ arrays in
src/sr_data (the files of Second Reality++, unchanged). The console cannot
keep them in RAM, so this tool:

  1. compiles a small PC program that includes those files and writes every
     array declared in their headers (and the Blob files) to a dump;
  2. writes build/gen/*.h: the same headers, where each big array is a
     reference (`extern const T (&name)[N]`) bound at start-up to its copy
     in the pack (sr_bind_data(), src/port/sr_pack.cpp), and each small one
     a plain array defined in build/gen/sr_ram_data.cpp (kept in RAM);
  3. writes build/gen/sr_flash_refs.cpp (the references) and the pack
     entries of the big arrays (sr_pack.py adds the music and writes the
     pack).

Usage: sr_data.py <src/sr_data> <build/gen> [--cxx g++] [--min-flash N]
"""
import argparse
import os
import re
import struct
import subprocess
import sys

# small arrays read in inner loops: in RAM whatever their size
RAM_ALWAYS = set()
# big arrays the demo writes to: none may go to flash (checked: the
# generated headers make every array const)

DECL = re.compile(r'^\s*extern\s+(const\s+)?([A-Za-z_][\w ]*?)\s+([A-Za-z_]\w*)\s*((?:\[[^\]]*\])*)\s*;')
SCALARS = {'char', 'unsigned char', 'short', 'unsigned short', 'int', 'unsigned int', 'int8_t', 'uint8_t',
           'int16_t', 'uint16_t', 'int32_t', 'uint32_t', 'size_t'}


def parse_header(path):
    ns = None
    decls = []
    with open(path) as f:
        lines = f.read().split('\n')
    for i, line in enumerate(lines):
        m = re.match(r'^\s*namespace\s+([\w:]+)', line)
        if m:
            ns = m.group(1)
        m = DECL.match(line)
        if m:
            const, typ, name, dims = m.groups()
            alldims = re.findall(r'\[([^\]]*)\]', dims)
            decls.append(dict(line=i, ns=ns, type=typ.strip(), name=name, inner=alldims[1:], full=ns + '::' + name,
                              scalar=not alldims))
    return lines, decls


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('src')
    ap.add_argument('gen')
    ap.add_argument('--cxx', default='g++')
    ap.add_argument('--min-flash', type=int, default=4096)
    args = ap.parse_args()

    src = os.path.abspath(args.src)
    gen = args.gen
    os.makedirs(gen, exist_ok=True)

    headers = []
    cpps = []
    for root, _, files in os.walk(src):
        for fn in sorted(files):
            p = os.path.join(root, fn)
            if fn.endswith('.h'):
                headers.append(p)
            elif fn.endswith('.cpp'):
                cpps.append(p)
    headers.sort()
    cpps.sort()

    parsed = {h: parse_header(h) for h in headers}
    arrays = [d for h in headers for d in parsed[h][1]]

    # the Blob files (read through Blob::open): no header
    blob_names = re.findall(r'unsigned char (_Blob_\d+)\[\]', open(os.path.join(src, 'Blob', 'Blob_Data.cpp')).read())

    # 1. the dumper
    dump_cpp = os.path.join(gen, 'sr_dump.cpp')
    with open(dump_cpp, 'w') as f:
        f.write('#include <cstdio>\n#include <cstdint>\n#include <cstring>\n')
        for c in cpps:
            f.write('#include "%s"\n' % c)
        f.write('static FILE *out;\n')
        f.write('static void dump(const char *n, const void *p, uint32_t size, uint32_t elem) {\n'
                '  uint32_t l = (uint32_t)strlen(n); fwrite(&l, 4, 1, out); fwrite(n, 1, l, out);\n'
                '  fwrite(&size, 4, 1, out); fwrite(&elem, 4, 1, out); fwrite(p, 1, size, out); }\n')
        f.write('int main(int argc, char **argv) {\n  out = fopen(argv[1], "wb"); if (!out) return 1;\n')
        for d in arrays:
            if d['scalar']:
                f.write('  dump("%s", &%s, sizeof(%s), sizeof(%s));\n' % (d['full'], d['full'], d['full'], d['full']))
            else:
                f.write('  dump("%s", %s, sizeof(%s), sizeof(%s[0]));\n' % (d['full'], d['full'], d['full'], d['full']))
        for b in blob_names:
            f.write('  dump("Blob::Data::%s", Blob::Data::%s, sizeof(Blob::Data::%s), 1);\n' % (b, b, b))
        f.write('  fclose(out); return 0;\n}\n')
    exe = os.path.join(gen, 'sr_dump')
    subprocess.check_call([args.cxx, '-std=c++20', '-O0', '-w', '-o', exe, dump_cpp])
    raw = os.path.join(gen, 'sr_dump.bin')
    subprocess.check_call([exe, raw])

    data = open(raw, 'rb').read()
    pos = 0
    dumped = {}
    while pos < len(data):
        (l,) = struct.unpack_from('<I', data, pos)
        pos += 4
        name = data[pos:pos + l].decode()
        pos += l
        size, elem = struct.unpack_from('<II', data, pos)
        pos += 8
        dumped[name] = (size, elem, data[pos:pos + size])
        pos += size

    def in_flash(d):
        size = dumped[d['full']][0]
        return not d['scalar'] and size >= args.min_flash and d['full'] not in RAM_ALWAYS

    def typed(d):  # a plain array in RAM (else a reference)
        return d['type'] in SCALARS and not in_flash(d)

    def dims(d):
        size, elem, _ = dumped[d['full']]
        return '[%d]' % (size // elem) + ''.join('[%s]' % x for x in d['inner'])

    # 2. headers
    for h in headers:
        lines, decls = parsed[h]
        lines = list(lines)
        for d in decls:
            if d['scalar']:
                continue
            if typed(d):
                lines[d['line']] = '    extern const %s %s%s;' % (d['type'], d['name'], dims(d))
            else:
                lines[d['line']] = '    extern const %s (&%s)%s; // %s' % (
                    d['type'], d['name'], dims(d), 'in the pack' if in_flash(d) else 'in RAM')
        with open(os.path.join(gen, os.path.basename(h)), 'w') as f:
            f.write('// generated by tools/sr_data.py from src/sr_data/%s\n' % os.path.relpath(h, src))
            f.write('\n'.join(lines))
            if not lines[-1].endswith('\n'):
                f.write('\n')

    fmt = {1: 'B', 2: 'H', 4: 'I', 8: 'Q'}

    def values(d, b, esize):
        return struct.unpack('<%d%s' % (len(b) // esize, fmt[esize]), b)

    # 3. RAM arrays and flash references
    with open(os.path.join(gen, 'sr_ram_data.cpp'), 'w') as f:
        f.write('// generated by tools/sr_data.py: the small arrays of the demo (in RAM)\n#include <stdint.h>\n#include <stddef.h>\n')
        for h in headers:
            f.write('#include "%s"\n' % os.path.basename(h))
        for d in arrays:
            if in_flash(d):
                continue
            size, elem, b = dumped[d['full']]
            if d['scalar']:
                v = int.from_bytes(b, 'little', signed=True)
                f.write('namespace %s { const %s %s = %d; }\n' % (d['ns'], d['type'], d['name'], v))
            elif typed(d):
                scal = 1
                for x in d['inner']:
                    scal *= int(x)
                esize = elem // scal
                vals = values(d, b, esize)
                f.write('namespace %s {\nconst %s %s%s = {\n' % (d['ns'], d['type'], d['name'], dims(d)))
                for i in range(0, len(vals), 16):
                    f.write('  ' + ', '.join('(%s)0x%X' % (d['type'], v) for v in vals[i:i + 16]) + ',\n')
                f.write('};\n}\n')
            else:
                raw = 'raw_' + d['full'].replace('::', '_')
                f.write('alignas(8) static const unsigned char %s[] = {\n' % raw)
                for i in range(0, len(b), 24):
                    f.write('  ' + ', '.join('0x%02X' % v for v in b[i:i + 24]) + ',\n')
                f.write('};\nconst %s::%s (&%s)%s = *reinterpret_cast<const %s::%s (*)%s>(%s);\n'
                        % (d['ns'], d['type'], d['full'], dims(d), d['ns'], d['type'], dims(d), raw))

    entries = []
    with open(os.path.join(gen, 'sr_flash_refs.cpp'), 'w') as f:
        f.write('// generated by tools/sr_data.py: the big arrays of the demo, in the pack\n'
                '// (bound by sr_bind_data(): this file\'s .init_array is renamed sr_init_array)\n'
                '#include <stdint.h>\n#include <stddef.h>\n#include "sr_pack.h"\n')
        for h in headers:
            f.write('#include "%s"\n' % os.path.basename(h))
        for d in arrays:
            if not in_flash(d):
                continue
            size, elem, b = dumped[d['full']]
            t = d['type'] if d['type'] in SCALARS else d['ns'] + '::' + d['type']
            f.write('const %s (&%s)%s = *reinterpret_cast<const %s (*)%s>(sr_pack_data("%s", %d));\n'
                    % (t, d['full'], dims(d), t, dims(d), d['full'], size))
            entries.append((d['full'], b))
    for bn in blob_names:
        full = 'Blob::Data::' + bn
        entries.append((full, dumped[full][2]))

    # the entries for the pack (sr_pack.py)
    with open(os.path.join(gen, 'sr_data.entries'), 'wb') as f:
        for name, b in entries:
            nb = name.encode()
            f.write(struct.pack('<I', len(nb)) + nb + struct.pack('<I', len(b)) + b)

    ram = sum(dumped[d['full']][0] for d in arrays if not in_flash(d))
    flash = sum(len(b) for _, b in entries)
    print('sr_data: %d arrays, %d in RAM (%d bytes), %d in the pack (%d bytes)'
          % (len(arrays) + len(blob_names), len(arrays) + len(blob_names) - len(entries), ram, len(entries), flash))


if __name__ == '__main__':
    main()
