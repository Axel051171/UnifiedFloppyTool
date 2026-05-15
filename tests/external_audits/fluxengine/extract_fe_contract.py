"""Extract the actual FluxEngine CLI contract."""
import re, json, pathlib, os

FE = pathlib.Path('/home/claude/fe')

# 1. Subcommands from fluxengine.cc
fe_main = (FE/'src/fluxengine.cc').read_text()
subcommands = re.findall(r'\{\s*"([a-z]+)"\s*,\s*main\w+', fe_main)

# 2. Flags from every fe-*.cc and lib/config/flags.cc
flag_files = list(FE.glob('src/fe-*.cc')) + [FE/'lib/config/flags.cc']
flags = {}
flag_pattern = re.compile(
    r'(?:String|Int|Bool|Action|Double)Flag\s+\w+\s*\(\s*\{([^}]+)\}',
    re.DOTALL
)
for f in flag_files:
    text = f.read_text()
    for m in flag_pattern.finditer(text):
        names = re.findall(r'"([^"]+)"', m.group(1))
        for n in names:
            flags.setdefault(n, []).append(f.name)

# 3. Profiles from src/formats/*.textpb
profiles = sorted([p.stem for p in (FE/'src/formats').glob('*.textpb')
                   if not p.stem.startswith('_')])

print(json.dumps({
    'subcommands': sorted(set(subcommands)),
    'flags': dict(sorted(flags.items())),
    'profiles_available': profiles,
}, indent=2))
