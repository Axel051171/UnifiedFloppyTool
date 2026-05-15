"""Compare UFT-advertised profiles vs FE's actual builtins."""
import re, json, pathlib

# Extract UFT's hardcoded profile list with descriptions
uft_src = pathlib.Path('/home/claude/uft/src/hardware_providers/fluxenginehardwareprovider.cpp').read_text()
# The block is multiple QStringLiteral("name"),  // Comment lines around line 556
prof_block = re.search(r'QStringLiteral\("ibm"\).*?QStringLiteral\("zilogmcz"\)', uft_src, re.DOTALL)
uft_profiles = re.findall(r'QStringLiteral\("([^"]+)"\)', prof_block.group(0)) if prof_block else []

fe = json.load(open('fe_contract.json'))
fe_profiles = set(fe['profiles_available'])
uft_set = set(uft_profiles)

print(f"UFT advertises: {len(uft_profiles)} profiles")
print(f"FE provides:    {len(fe_profiles)} profiles")
print(f"\nBoth:        {sorted(uft_set & fe_profiles)}")
print(f"\nUFT-only:    {sorted(uft_set - fe_profiles)} — UFT lists these but FE doesn't have them")
print(f"\nFE-only:     {sorted(fe_profiles - uft_set)} — UFT could expose these but doesn't")
