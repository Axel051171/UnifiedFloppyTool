"""Extract actual fluxengine CLI invocations from UFT source."""
import re, json, pathlib

src = pathlib.Path('/home/claude/uft/src/hardware_providers/fluxenginehardwareprovider.cpp').read_text()

# Find all QStringList args = { ... } blocks plus runFluxEngine({...}) inline lists
pattern = re.compile(
    r'(?:QStringList\s+args\s*=\s*\{|runFluxEngine\s*\(\s*\{)([^}]+)\}',
    re.DOTALL
)
calls = []
for m in pattern.finditer(src):
    body = m.group(1)
    tokens = re.findall(r'QStringLiteral\(\s*"([^"]*)"\s*\)', body)
    if not tokens: continue
    calls.append(tokens)

# Find profile lists (the UFT-supported -c values)
prof_block = re.search(r'profiles\s*<<.*?;', src, re.DOTALL)
profiles = re.findall(r'QStringLiteral\(\s*"([^"]+)"\s*\)', prof_block.group(0)) if prof_block else []

print(json.dumps({'calls': calls, 'profiles_advertised': profiles}, indent=2))
