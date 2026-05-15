"""Extract UFT protocol constants from the C headers by regex."""
import re, json, pathlib

ROOT = pathlib.Path('/home/claude/uft')
hdrs = [
    ROOT/'include/uft/gw_protocol.h',
    ROOT/'include/uft/hal/uft_greaseweazle_full.h',
]
text = '\n'.join(p.read_text() for p in hdrs)

def find(prefix, exclude=()):
    out = {}
    for m in re.finditer(rf'#define\s+({prefix}[A-Z0-9_]+)\s+(?:\(?)((?:0x[0-9A-Fa-f]+|\d+))', text):
        name, val = m.group(1), m.group(2)
        if any(e in name for e in exclude): continue
        out[name] = int(val, 0)
    return out

cmd  = find('CMD_',          exclude=('CMD_MAX',))
ack  = find('ACK_')
bus  = find('BUS_')
fop  = find('FLUXOP_')

print(json.dumps({'cmd':cmd,'ack':ack,'bustype':bus,'fluxop':fop}, indent=2))
