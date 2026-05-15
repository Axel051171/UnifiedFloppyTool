"""Extract Greaseweazle protocol constants from the official Python source."""
import sys, json, importlib.util, os
sys.path.insert(0, '/home/claude/gw/src')

# stub the optimised C module so import doesn't blow up
import types
mod = types.ModuleType('greaseweazle.optimised')
sys.modules['greaseweazle.optimised'] = mod
mod_err = types.ModuleType('greaseweazle.error')
class _Fatal(Exception): pass
mod_err.Fatal = _Fatal
sys.modules['greaseweazle.error'] = mod_err
mod_flux = types.ModuleType('greaseweazle.flux')
mod_flux.Flux = object
sys.modules['greaseweazle.flux'] = mod_flux

from greaseweazle import usb as gw

out = {
  'cmd':     {k:v for k,v in gw.Cmd.__dict__.items()    if isinstance(v,int)},
  'ack':     {k:v for k,v in gw.Ack.__dict__.items()    if isinstance(v,int)},
  'bustype': {e.name:e.value for e in gw.BusType},
  'fluxop':  {k:v for k,v in gw.FluxOp.__dict__.items() if isinstance(v,int)},
  'earliest_fw': list(gw.EARLIEST_SUPPORTED_FIRMWARE),
}
print(json.dumps(out, indent=2))
