"""Test UFT's regex against actual FE rpm output."""
import re, subprocess

# UFT's regex from fluxenginehardwareprovider.cpp:484
UFT_RPM_REGEX = re.compile(r'([0-9.]+)\s*rpm', re.IGNORECASE)

# Actual FE outputs (from src/fe-rpm.cc):
fe_outputs = {
    'normal':  "Rotational period is 200.0 ms (300.0 rpm)",
    'fast':    "Rotational period is 166.67 ms (360.0 rpm)",
    'no_disk': "No index pulses detected; is a disk in the drive?",
    'old_fmt': "300 rpm",            # hypothetical older format
    'german':  "Drehperiode: 200ms (300 U/min)",  # if locale changed
}
for label, out in fe_outputs.items():
    m = UFT_RPM_REGEX.search(out)
    if m:
        print(f"[{label}] '{out[:60]}'")
        print(f"         → UFT parses: {m.group(1)} rpm  ✓")
    else:
        print(f"[{label}] '{out[:60]}'")
        print(f"         → UFT parses: NO MATCH  ✗ — drive detection would fail")
