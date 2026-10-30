#!/usr/bin/env python3
"""
make check: the PC build plays the demo (virtual time, 700 s: all the parts
and the start of the end scroller) and the pictures shown are compared with
tests/expected.txt (a CRC per picture, summed up per part). The values come
from the port's first version, with Second Reality++'s code before the
memory changes made for the console: a change that changes a picture shows
here.

  check.py <host binary> <pack> [--update]
"""
import hashlib
import os
import subprocess
import sys
import tempfile

SECONDS = 700


def run(binary, pack):
    with tempfile.TemporaryDirectory() as d:
        bin_path = os.path.join(d, 'SecondReality.bin')
        open(bin_path, 'wb').close()
        os.symlink(os.path.abspath(pack), os.path.join(d, 'SecondReality.srd'))
        hashes = os.path.join(d, 'hash.txt')
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', SR_FAST='1',
                   SR_SECONDS=str(SECONDS), SR_HASH=hashes)
        subprocess.run([os.path.abspath(binary), bin_path], cwd=d, env=env, check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        parts = {}
        for line in open(hashes):
            vbl, part, crc = line.split()
            p = parts.setdefault(int(part), [0, hashlib.sha1(), int(vbl)])
            p[0] += 1
            p[1].update(line.encode())
        return ['part %2d: first picture at VBL %6d, %5d pictures, %s' % (k, v[2], v[0], v[1].hexdigest())
                for k, v in sorted(parts.items())]


def main():
    binary, pack = sys.argv[1], sys.argv[2]
    expected_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'expected.txt')
    got = run(binary, pack)
    if '--update' in sys.argv:
        open(expected_path, 'w').write('\n'.join(got) + '\n')
        print('check: %s written' % expected_path)
        return
    expected = open(expected_path).read().split('\n')[:len(got)]
    bad = [(e, g) for e, g in zip(expected, got) if e != g]
    if bad or len(expected) != len(got):
        for e, g in bad:
            print('check: expected %s\n          got %s' % (e, g))
        sys.exit('check: FAILED')
    print('check: %d parts, the same pictures as expected' % len(got))


if __name__ == '__main__':
    main()
