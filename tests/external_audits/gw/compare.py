"""Cross-validate UFT protocol constants against Greaseweazle reference."""
import json, re

gw  = json.load(open('gw_constants.json'))
uft = json.load(open('uft_constants.json'))

def norm_cmd(s): return re.sub(r'[_]', '', s).upper().replace('CMD','').strip('_')
def norm_ack(s): return re.sub(r'[_]', '', s).upper().replace('ACK','').replace('OKAY','OKAY').strip('_')

# Map UFT names to GW names case-insensitively
def cmp_section(name, gw_d, uft_d, strip):
    g_norm = {re.sub(r'[_\s]','',k).upper(): v for k,v in gw_d.items()}
    u_norm = {re.sub(r'[_\s]','',k).upper().replace(strip,'',1): v for k,v in uft_d.items()}
    all_keys = sorted(set(g_norm) | set(u_norm))
    ok, fail = [], []
    for k in all_keys:
        g, u = g_norm.get(k), u_norm.get(k)
        if g is None:    fail.append(('UFT-only', k, '-', u))
        elif u is None:  fail.append(('GW-only',  k, g, '-'))
        elif g != u:     fail.append(('MISMATCH', k, g, u))
        else:            ok.append((k, g))
    print(f"\n=== {name} ===")
    print(f"  {len(ok)} matching, {len(fail)} divergent")
    for tag, k, g, u in fail: print(f"  [{tag}] {k}: gw={g}, uft={u}")
    return len(fail)

failures = 0
failures += cmp_section('CMD opcodes',  gw['cmd'],     uft['cmd'],     'CMD')
failures += cmp_section('ACK codes',    gw['ack'],     uft['ack'],     'ACK')
failures += cmp_section('BusType',      gw['bustype'], uft['bustype'], 'BUS')
failures += cmp_section('FluxOp',       gw['fluxop'],  uft['fluxop'],  'FLUXOP')

print(f"\n>>> Total divergences: {failures}")
