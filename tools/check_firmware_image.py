#!/usr/bin/env python3
"""Reject CH552 HEX images that touch the ISP BootLoader (0x3800..0x3fff)."""
import argparse
from pathlib import Path


def validate_hex(text, limit=0x3800):
    memory, base, eof = {}, 0, False
    for number, line in enumerate(text.splitlines(), 1):
        line = line.strip()
        if not line:
            continue
        if eof or not line.startswith(':'):
            raise ValueError(f'line {number}: invalid HEX record position')
        record = bytes.fromhex(line[1:])
        if len(record) < 5 or len(record) != record[0] + 5 or sum(record) & 255:
            raise ValueError(f'line {number}: invalid length/checksum')
        length, kind = record[0], record[3]
        offset = int.from_bytes(record[1:3], 'big')
        data = record[4:-1]
        if kind == 0:
            for index, value in enumerate(data):
                address = base + offset + index
                if address >= limit:
                    raise ValueError(f'HEX touches reserved ISP space: 0x{address:04X}')
                if address in memory and memory[address] != value:
                    raise ValueError(f'conflicting data at 0x{address:04X}')
                memory[address] = value
        elif kind == 1 and length == 0:
            eof = True
        elif kind in (2, 4) and length == 2:
            base = int.from_bytes(data, 'big') << (4 if kind == 2 else 16)
        elif kind in (3, 5) and length == 4:
            pass
        else:
            raise ValueError(f'line {number}: unsupported HEX record')
    if not eof or not memory:
        raise ValueError('missing EOF or empty image')
    return {'bytes': len(memory), 'max_address': max(memory), 'headroom': limit - max(memory) - 1}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('hexfile', type=Path)
    args = parser.parse_args()
    try:
        result = validate_hex(args.hexfile.read_text(encoding='ascii'))
    except (ValueError, OSError) as exc:
        parser.exit(1, f'Firmware image: FAIL: {exc}\n')
    print(f"Firmware image: PASS, {result['bytes']} bytes, max 0x{result['max_address']:04X}, ISP headroom {result['headroom']} bytes")
