#!/usr/bin/env python3
"""Mock fluxengine binary — accepts only what the REAL fluxengine accepts,
prints what would happen. Use this to test UFT's actual invocations."""
import sys, json, pathlib

KNOWN_SUBCOMMANDS = {'rpm','read','write','rawwrite','convert','seek','format',
                     'inspect','analyse','ls','cp','mv','rm','mkdir','getfile',
                     'putfile','getfileinfo','getdiskinfo','fluxfile','test'}
KNOWN_FLAGS = {  # flag -> takes_value?
    '-s': True, '--source': True,
    '-o': True, '--output': True,
    '-d': True, '--dest': True,
    '-i': True, '--input': True,
    '-c': True, '--config': True,    # IMPORTANT: this is config/profile, not cylinder!
    '-t': True, '--cylinder': True,  # cylinder flag (only for seek)
    '--copy-flux-to': True,
    '--tracks': True,
    '--drive.revolutions': True,
    '--drive.sync_with_index': True,
    '--drive.index_mode': True,
    '--version': False, '--help': False, '--doc': False,
}
KNOWN_PROFILES = {'acornadfs','acorndfs','aeslanier','agat','amiga','ampro',
                  'apple2','atarist','bk','brother','commodore','eco1','epsonpf10',
                  'f85','fb100','hplif','ibm','icl30','juku','mac','micropolis',
                  'ms2000','mx','n88basic','northstar','psos','rolandd20','rx50',
                  'smaky6','tartu','ti99','victor9k','zilogmcz'}

errors = []
warnings = []
args = sys.argv[1:]
print(f"[mock-fluxengine] argv = {args!r}", file=sys.stderr)

if not args:
    errors.append("no subcommand")
elif args[0].startswith('--'):
    if args[0] not in KNOWN_FLAGS:
        errors.append(f"unknown top-level flag: {args[0]}")
elif args[0] not in KNOWN_SUBCOMMANDS:
    errors.append(f"unknown subcommand: {args[0]}")
else:
    sub = args[0]
    i = 1
    while i < len(args):
        tok = args[i]
        if tok.startswith('-'):
            flag = tok.split('=',1)[0]
            if flag not in KNOWN_FLAGS:
                errors.append(f"unknown flag for '{sub}': {flag}")
                i += 1
                continue
            # consume value
            if '=' not in tok and KNOWN_FLAGS[flag] and i+1 < len(args):
                val = args[i+1]
                # IMPORTANT: -c takes a profile NAME — if UFT passes a number,
                # that's the cylinder-vs-config conflict.
                if flag in ('-c','--config'):
                    if val.isdigit():
                        errors.append(
                          f"-c {val}: '-c' loads a config/profile by name; "
                          f"got numeric value '{val}'. "
                          f"UFT likely intended --tracks=c{val}h0 instead.")
                    elif val not in KNOWN_PROFILES and not val.endswith('.textpb'):
                        warnings.append(f"-c {val}: not a known builtin profile")
                i += 2
            else:
                i += 1
        else:
            # bare positional
            if sub in ('read','write','rawwrite'):
                errors.append(
                  f"unexpected positional '{tok}' after '{sub}': "
                  f"profile must be passed via -c {tok}")
            i += 1

# Simulate stdout for 'rpm' so UFT's parser sees something
if not errors and args and args[0] == 'rpm':
    print("Rotational period is 200 ms (300.0 rpm)")

print(f"\n[mock-fluxengine] {len(errors)} error(s), {len(warnings)} warning(s)", file=sys.stderr)
for e in errors:   print(f"  ERROR: {e}",   file=sys.stderr)
for w in warnings: print(f"  WARN:  {w}",   file=sys.stderr)
sys.exit(1 if errors else 0)
