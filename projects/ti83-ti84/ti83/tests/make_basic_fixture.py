"""Create an original BASIC import/auto-launch fixture."""
import pathlib
import struct
import sys

code = b'\xe1\x3f\xde\x2aBASICOK\x2a\x3f\xd8\x3f'
data = struct.pack('<H', len(code)) + code
entry = struct.pack('<HHB8sBBH', 13, len(data), 5, b'LOADTEST', 0, 0, len(data)) + data
header = b'**TI83F*\x1a\x0a\0' + b'TI83 host import test'.ljust(42, b'\0')
pathlib.Path(sys.argv[1]).write_bytes(
    header + struct.pack('<H', len(entry)) + entry + struct.pack('<H', sum(entry) & 65535)
)
