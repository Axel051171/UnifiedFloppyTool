import numpy as np

REV_TIME_NS_300 = 200000000  # ~200ms at 300 RPM
REV_TIME_NS_360 = 166666667  # ~166.67ms at 360 RPM


def _bits_per_rev(base_cell_ns: float, rpm: float) -> int:
    rev_time = REV_TIME_NS_300 if rpm == 300 else REV_TIME_NS_360
    return max(1, int(rev_time / max(1.0, base_cell_ns)))


def _normalize_revolution(flux: list, rpm: float, base_cell_ns: float) -> list:
    """Pad or trim to approx one revolution time using half-cell padding.
    """
    rev_time = REV_TIME_NS_300 if rpm == 300 else REV_TIME_NS_360
    s = int(sum(flux))
    if s < rev_time:
        half = int(max(1.0, base_cell_ns / 2.0))
        remaining = rev_time - s
        n = remaining // half
        # keep even count for Manchester-like pairs
        if n % 2 != 0:
            n -= 1
        flux += [half, half] * (n // 2)
    elif s > rev_time:
        # Trim from end (rare)
        cut = s - rev_time
        while cut > 0 and flux:
            cut -= flux.pop()
    return flux


def gen_random(revolutions: int, base_cell_ns: float, rpm: float = 360.0, seed: int | None = None) -> list:
    rng = np.random.default_rng(seed)
    per_rev = _bits_per_rev(base_cell_ns, rpm)
    lo = int(max(1.0, base_cell_ns * 0.5))
    hi = int(max(2, base_cell_ns * 2.0))
    out = []
    for _ in range(revolutions):
        rev = rng.integers(lo, hi + 1, size=per_rev, dtype=np.int32).tolist()
        rev = _normalize_revolution(rev, rpm, base_cell_ns)
        out.extend(rev)
    return out


def gen_prbs(revolutions: int, base_cell_ns: float, order: int = 7, rpm: float = 360.0, seed: int | None = None) -> list:
    # Simple LFSR; taps for 7/15 are commonly used
    taps = {
        7: (7, 6),     # x^7 + x^6 + 1
        15: (15, 14),  # x^15 + x^14 + 1
    }
    t = taps.get(order, (7, 6))
    state = (1 << (order - 1)) if seed is None else (seed & ((1 << order) - 1)) or 1
    long_cell = int(base_cell_ns * 1.2)
    short_cell = int(base_cell_ns * 0.8)
    per_rev = _bits_per_rev(base_cell_ns, rpm)
    out = []
    for _ in range(revolutions):
        rev = []
        for _i in range(per_rev):
            bit = state & 1
            # feedback
            fb = ((state >> (t[0]-1)) ^ (state >> (t[1]-1))) & 1
            state = (state >> 1) | (fb << (order - 1))
            rev.append(long_cell if bit else short_cell)
        rev = _normalize_revolution(rev, rpm, base_cell_ns)
        out.extend(rev)
    return out


def gen_alt(revolutions: int, base_cell_ns: float, rpm: float = 360.0, runlen: int = 1) -> list:
    long_cell = int(base_cell_ns * 1.5)
    short_cell = int(base_cell_ns)
    per_rev = _bits_per_rev(base_cell_ns, rpm)
    out = []
    toggle = False
    for _ in range(revolutions):
        rev = []
        count = 0
        for _i in range(per_rev):
            rev.append(long_cell if toggle else short_cell)
            count += 1
            if count >= runlen:
                toggle = not toggle
                count = 0
        rev = _normalize_revolution(rev, rpm, base_cell_ns)
        out.extend(rev)
    return out


def gen_runlen(revolutions: int, base_cell_ns: float, rpm: float = 360.0, max_len: int = 8, seed: int | None = None) -> list:
    rng = np.random.default_rng(seed)
    long_cell = int(base_cell_ns * 1.6)
    short_cell = int(base_cell_ns * 0.9)
    per_rev = _bits_per_rev(base_cell_ns, rpm)
    out = []
    for _ in range(revolutions):
        rev = []
        val = short_cell
        remain = per_rev
        while remain > 0:
            run = int(rng.integers(1, max_len + 1))
            for _k in range(min(run, remain)):
                rev.append(val)
            val = long_cell if val == short_cell else short_cell
            remain -= run
        rev = _normalize_revolution(rev, rpm, base_cell_ns)
        out.extend(rev)
    return out


def gen_chirp(revolutions: int, base_cell_ns: float, rpm: float = 360.0, start_ns: float = None, end_ns: float = None) -> list:
    start = int(start_ns or (base_cell_ns * 0.6))
    end = int(end_ns or (base_cell_ns * 1.8))
    per_rev = _bits_per_rev(base_cell_ns, rpm)
    out = []
    for _ in range(revolutions):
        rev = np.linspace(start, end, per_rev, dtype=np.int32).tolist()
        rev = _normalize_revolution(rev, rpm, base_cell_ns)
        out.extend(rev)
    return out


def gen_dc_bias(revolutions: int, base_cell_ns: float, rpm: float = 360.0, bias: float = 0.1) -> list:
    # bias: -0.5..+0.5 modifies duty; >0 skews longer
    per_rev = _bits_per_rev(base_cell_ns, rpm)
    out = []
    for _ in range(revolutions):
        rev = []
        for i in range(per_rev):
            phase = (i / max(1, per_rev-1)) - 0.5
            scale = 1.0 + bias * phase
            rev.append(int(max(1.0, base_cell_ns * scale)))
        rev = _normalize_revolution(rev, rpm, base_cell_ns)
        out.extend(rev)
    return out


def gen_burst(revolutions: int, base_cell_ns: float, rpm: float = 360.0, period: int = 50, duty: float = 0.5, noise: float = 0.25) -> list:
    per_rev = _bits_per_rev(base_cell_ns, rpm)
    out = []
    for _ in range(revolutions):
        rev = []
        for i in range(per_rev):
            in_burst = (i % max(1, period)) < int(period * duty)
            base = base_cell_ns * (0.9 if in_burst else 1.2)
            jitter = np.random.normal(0, base_cell_ns * noise)
            rev.append(int(max(1.0, base + jitter)))
        rev = _normalize_revolution(rev, rpm, base_cell_ns)
        out.extend(rev)
    return out


def generate_pattern(name: str, revolutions: int, base_cell_ns: float, rpm: float = 360.0, **kwargs) -> list:
    name = (name or 'random').lower()
    if name == 'random':
        return gen_random(revolutions, base_cell_ns, rpm, seed=kwargs.get('seed'))
    if name == 'prbs7':
        return gen_prbs(revolutions, base_cell_ns, 7, rpm, seed=kwargs.get('seed'))
    if name == 'prbs15':
        return gen_prbs(revolutions, base_cell_ns, 15, rpm, seed=kwargs.get('seed'))
    if name == 'alt':
        return gen_alt(revolutions, base_cell_ns, rpm, runlen=int(kwargs.get('runlen', 1)))
    if name == 'runlen':
        return gen_runlen(revolutions, base_cell_ns, rpm, max_len=int(kwargs.get('max_len', 8)), seed=kwargs.get('seed'))
    if name == 'chirp':
        return gen_chirp(revolutions, base_cell_ns, rpm, start_ns=kwargs.get('chirp_start_ns'), end_ns=kwargs.get('chirp_end_ns'))
    if name == 'dc_bias':
        return gen_dc_bias(revolutions, base_cell_ns, rpm, bias=float(kwargs.get('dc_bias', 0.1)))
    if name == 'burst':
        return gen_burst(revolutions, base_cell_ns, rpm, period=int(kwargs.get('burst_period', 50)), duty=float(kwargs.get('burst_duty', 0.5)), noise=float(kwargs.get('burst_noise', 0.25)))
    # default
    return gen_random(revolutions, base_cell_ns, rpm, seed=kwargs.get('seed'))
