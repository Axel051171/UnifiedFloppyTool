"""Diff UFT's CLI usage against actual FluxEngine contract."""
import json, re, subprocess

fe = json.load(open('fe_contract.json'))
calls = json.loads(subprocess.check_output(['python3','extract_uft_calls.py']))['calls']

# Reconstruct each UFT call into a check
print("="*72)
print("UFT → FluxEngine CLI call analysis")
print("="*72)

for i, call in enumerate(calls, 1):
    print(f"\n[{i}] UFT calls: fluxengine {' '.join(call)}")
    if not call: continue
    subcmd = call[0]

    # Subcommand check
    if subcmd.startswith('--'):
        if subcmd[2:] in ('version','help','doc'):
            print(f"    subcommand: -- (top-level flag) ... OK")
        else:
            print(f"    subcommand: {subcmd} ... UNKNOWN")
    elif subcmd in fe['subcommands']:
        print(f"    subcommand '{subcmd}' .................. ✓ exists")
    else:
        print(f"    subcommand '{subcmd}' .................. ✗ NOT IN FE")

    # Flag check — every token starting with -
    for tok in call[1:]:
        if not tok.startswith('-'):
            # positional. only `read` takes none — `read ibm` is wrong.
            if subcmd in ('read','write'):
                print(f"    positional '{tok}' ....................... ⚠ FE expects no positional here (use -c {tok})")
            continue
        flag = tok.split('=')[0]
        if flag in fe['flags']:
            files = fe['flags'][flag]
            # is the flag bound to a different subcommand than the one called?
            applicable = any(subcmd in f or 'flags.cc' in f for f in files)
            note = "OK" if applicable else f"defined for {files}"
            print(f"    flag '{flag}' .... ✓ found ({note})")
        else:
            print(f"    flag '{flag}' .... ✗ NOT A FLUXENGINE FLAG")
