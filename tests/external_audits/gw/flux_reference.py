"""Generate reference flux byte streams from official GW _encode_flux,
then compare against UFT's C encoder output."""
import sys, struct
sys.path.insert(0, '/home/claude/gw/src')

# Stub deps
import types
for n in ('greaseweazle.optimised',):
    sys.modules[n] = types.ModuleType(n)
me = types.ModuleType('greaseweazle.error'); me.Fatal=Exception; sys.modules['greaseweazle.error']=me
mf = types.ModuleType('greaseweazle.flux'); mf.Flux=object;   sys.modules['greaseweazle.flux']=mf

from greaseweazle.usb import Unit, FluxOp

# Borrow only the _encode_flux logic, simulate as method on a fake object
class FakeUnit:
    sample_freq = 72_000_000
    _encode_flux = Unit._encode_flux

# Test vectors covering every branch in the encoder:
tests = {
    'small':       [100, 150, 200, 248, 249],            # < 250 direct
    'two_byte':    [250, 500, 1000, 1500, 1524],         # 2-byte form
    'three_form':  [1525, 2000, 5000, 10000],            # transitional
    'nfa':         [20_000_000],                          # > 150us threshold -> Space+Astable
    'mixed':       [100, 250, 1500, 2500, 100, 50, 300],
    'zeros':       [0, 100, 0, 200, 0],                   # zeros must be skipped
}
import json
out = {}
for name, vec in tests.items():
    enc = bytes(FakeUnit()._encode_flux(vec))
    out[name] = {'input': vec, 'expected_bytes': enc.hex()}
    print(f"{name}: {len(vec)} samples -> {len(enc)} bytes : {enc.hex()}")

json.dump(out, open('flux_vectors.json','w'), indent=2)
