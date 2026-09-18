#!/usr/bin/env python3
"""A VHS presentation of verbatim recorded ESP32 requests and results.

No inference is simulated here. Run capture.py to obtain the recording first.
"""
import json
from pathlib import Path
import sys
import textwrap
import time

ROOT = Path(__file__).resolve().parent
RESET = '\033[0m'
BOLD = '\033[1m'
HIDE = '\033[?25l'
CLEAR = '\033[2J\033[H'


def rgb(h):
    return '\033[38;2;' + ';'.join(str(int(h[i:i+2], 16)) for i in (0, 2, 4)) + 'm'


PINK, PURPLE, CYAN, GREEN, ORANGE, WHITE, DIM = map(rgb, ['ff79c6', 'bd93f9', '8be9fd', '50fa7b', 'ffb86c', 'f8f8f2', '9699af'])
ROUTES = [('get_status', 'STATUS', 'uptime + free memory'),
          ('set_sampling_interval', 'TELEMETRY', 'sampling cadence'),
          ('set_timer', 'TIMER', 'countdown schedule')]


def put(row, text='', col=1):
    sys.stdout.write(f'\033[{row};{col}H\033[K{text}{RESET}')
    sys.stdout.flush()


def base(section):
    sys.stdout.write(CLEAR + HIDE)
    put(2, f'{BOLD}{PINK}NEEDLE 3{RESET}{WHITE}  /  THE EDGE TASK ROUTER')
    put(3, f'{DIM}ESP32-S3 · 8 layers · 16.2 MB · 240 MHz')
    put(5, f'{PURPLE}{"━" * 49}')
    put(6, f'{CYAN}{section}')
    put(28, f'{DIM}Actual board capture · inference waits condensed')
    put(29, f'{DIM}HTTP → USB bridge → model + handlers on ESP32')


def intro():
    base('PLAIN ENGLISH → LOCAL ACTION')
    glyphs = [
        '████   ███  █   █ █████ █████',
        '█   █ █   █ █   █   █   █    ',
        '████  █   █ █   █   █   ████ ',
        '█  █  █   █ █   █   █   █    ',
        '█   █  ███   ███    █   █████',
    ]
    for i, line in enumerate(glyphs):
        put(9+i, f'{PINK if i < 2 else PURPLE}      {line}')
        time.sleep(.10)
    put(17, f'{BOLD}{WHITE}One tiny model. Three real jobs.')
    put(19, f'{CYAN}01  OBSERVE     {WHITE}Read board health')
    put(21, f'{PURPLE}02  CONFIGURE   {WHITE}Change telemetry sampling')
    put(23, f'{PINK}03  SCHEDULE    {WHITE}Start hardware timers')
    put(26, f'{GREEN}No cloud inference. No extra sensors required.')
    time.sleep(3)


def display_call(call):
    args = ', '.join(f'{k}={json.dumps(v)}' for k, v in call['arguments'].items())
    return f"{call['name']}({args})"


def scene(case, index, total):
    response = case['response']
    calls = response['function_calls']
    state = case['state_after']
    base(f"{index:02d}/{total:02d}  {['READ DEVICE HEALTH','RECONFIGURE TELEMETRY','START A COUNTDOWN','ONE REQUEST → TWO ACTIONS','CHECK THE CHANGED DEVICE'][index-1]}")
    put(8, f'{PINK}YOU')
    # Type the actual captured prompt. The reply below comes from the recording.
    lines = textwrap.wrap(case['input'], width=49)
    for i, line in enumerate(lines):
        for n in range(1, len(line)+1):
            put(9+i, f'{BOLD}{WHITE}{line[:n]}')
            time.sleep(.023)
    time.sleep(.3)
    put(12, f'{PURPLE}               ┌──────────────────┐')
    put(13, f'{PURPLE}               │{WHITE} NEEDLE 3 / ESP32 {PURPLE}│')
    put(14, f'{PURPLE}               └──────────────────┘')
    selected = {call['name'] for call in calls}
    for i, (name, label, detail) in enumerate(ROUTES):
        color = GREEN if name in selected else DIM
        mark = '●' if name in selected else '·'
        put(16+i, f'{color}{mark} {label:<11} {detail}')
        time.sleep(.17)
    put(20, f'{CYAN}MODEL SELECTED  {DIM}(validated + executed on board)')
    for i, call in enumerate(calls):
        put(21+i, f'{GREEN}→ {display_call(call)}')
    if case['id'] in ('status', 'status_after'):
        detail = f"RAM {state['free_internal_bytes']/1024:.0f} KiB  |  {state['samples']} samples  |  timer {state['timer_status']}"
    elif case['id'] == 'sampling':
        detail = f"Sampling period changed to {state['sample_period_s']} seconds"
    elif case['id'] == 'timer':
        detail = f"Countdown running: {state['timer_remaining_ms']/1000:.1f}s remaining"
    else:
        detail = f"Cadence {state['sample_period_s']}s  +  countdown {state['timer_status']}"
    put(24, f'{WHITE}{detail}')
    put(26, f"{PINK}{response['latency_ms']/1000:.1f}s{WHITE} actual request   {CYAN}{response['decode_tps']:.2f}{WHITE} tok/s decode")
    time.sleep(3.0)


def outro(doc):
    base('THE DEVICE KEPT WORKING THROUGH INFERENCE')
    before, after = doc['before'], doc['after']
    put(9, f'{GREEN}✓  3 different handlers selected')
    put(11, f'{GREEN}✓  2 actions from one sentence')
    put(13, f"{GREEN}✓  {after['samples']-before['samples']} telemetry samples collected")
    put(15, f"{GREEN}✓  Countdown expired on the ESP32")
    put(18, f'{BOLD}{WHITE}A local control plane for a tiny device.')
    put(21, f'{ORANGE}Known limit: unrelated prompts can misroute.')
    negative = next(c for c in doc['cases'] if c['id'] == 'unsupported')
    if not negative['exact_match']:
        actual = ', '.join(c['name'] for c in negative['response']['function_calls'])
        put(22, f'{DIM}Weather query → {actual} (incorrect)')
    put(25, f'{PURPLE}github.com/iammrduncan/esp32-needle-3')
    time.sleep(6)
    put(27, f'{DIM}DEMO COMPLETE')


def main():
    doc = json.loads((ROOT/'recording.json').read_text())
    if not all(v for k, v in doc['verification'].items() if k != 'unsupported_rejected'):
        raise SystemExit('Capture verification failed. Do not render a success demo.')
    cases = [c for c in doc['cases'] if c['id'] != 'unsupported']
    intro()
    for i, case in enumerate(cases, 1):
        scene(case, i, len(cases))
    outro(doc)
    sys.stdin.readline()


if __name__ == '__main__':
    try:
        main()
    finally:
        sys.stdout.write(RESET + '\033[?25h\n')
