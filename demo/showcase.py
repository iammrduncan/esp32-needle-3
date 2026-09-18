#!/usr/bin/env python3
"""VHS presentation of real model choices and two-pass ESP32 tool execution."""
import json
from pathlib import Path
import sys
import textwrap
import time

ROOT = Path(__file__).resolve().parent
RESET, BOLD, HIDE, CLEAR = '\033[0m', '\033[1m', '\033[?25l', '\033[2J\033[H'


def rgb(h):
    return '\033[38;2;' + ';'.join(str(int(h[i:i+2], 16)) for i in (0, 2, 4)) + 'm'


PINK, PURPLE, CYAN, GREEN, ORANGE, WHITE, DIM = map(rgb, ['ff79c6', 'bd93f9', '8be9fd', '50fa7b', 'ffb86c', 'f8f8f2', '9699af'])


def put(row, text=''):
    sys.stdout.write(f'\033[{row};1H\033[K{text}{RESET}')
    sys.stdout.flush()


def base(section, board):
    sys.stdout.write(CLEAR + HIDE)
    put(2, f'{BOLD}{PINK}NEEDLE 3{RESET}{WHITE}  /  THE AGENT WATCH BRAIN')
    put(3, f"{DIM}ESP32-S3 · {board['layers']} layers · {board['model_bytes']/1e6:.1f} MB · 240 MHz")
    put(5, f'{PURPLE}{"━" * 52}')
    put(6, f'{CYAN}{section}')
    put(28, f'{DIM}Real ESP32 capture · inference waits condensed')
    put(29, f'{DIM}Watch concept · HTTP via USB · no external LLM calls')


def prompt(text, animate=True):
    put(8, f'{PINK}YOU → YOUR AGENT WATCH')
    for i, line in enumerate(textwrap.wrap(text, width=52)):
        if animate:
            for n in range(1, len(line)+1):
                put(9+i, f'{BOLD}{WHITE}{line[:n]}')
                time.sleep(.017)
        else:
            put(9+i, f'{BOLD}{WHITE}{line}')


def intro(doc):
    base('WHICH MODEL SHOULD HANDLE THIS?', doc['before'])
    put(9, f'{WHITE}Your request arrives on a tiny watch.')
    put(12, f'{PURPLE}            ┌────────────────────┐')
    put(13, f'{PURPLE}            │ {PINK}NEEDLE 3 / ESP32   {PURPLE}│')
    put(14, f'{PURPLE}            │ {WHITE}pick the model     {PURPLE}│')
    put(15, f'{PURPLE}            └────────────────────┘')
    put(18, f'{CYAN}  EXTERNAL                   {GREEN}LOCAL')
    put(20, f'{WHITE}  Qwen / GPT OSS / Opus       {GREEN}Needle again')
    put(22, f'{DIM}  Show selection. Stop.       {GREEN}Generate tools.')
    put(23, f'{DIM}                             {GREEN}Execute. Verify.')
    put(26, f'{PINK}One small model decides what happens next.')
    time.sleep(4)


def scene(doc, case, index):
    response = case['response']
    selected = response['selected_model']
    choice = selected['key']
    base(f'{index:02d}/{len(doc["cases"]):02d}  PASS 1 — CHOOSE THE MODEL', doc['before'])
    prompt(case['input'])
    put(12, f'{PURPLE}NEEDLE ON ESP32 → configured model choices')
    roles = {'needle': 'on-watch tools', 'qwen': 'writing / language',
             'gpt_oss': 'code / analysis', 'opus': 'research / design'}
    for i, (key, model) in enumerate(doc['catalog'].items()):
        put(14+i, f'{DIM}  {model["label"]:<22} {roles[key]}')
    time.sleep(.6)
    for i, (key, model) in enumerate(doc['catalog'].items()):
        color = GREEN if key == choice else DIM
        mark = '●' if key == choice else '·'
        put(14+i, f'{color}{mark} {model["label"]:<22} {roles[key]}')
    route_name = response['routing']['function_calls'][0]['name']
    put(20, f'{CYAN}SELECTED → {BOLD}{selected["label"]}')
    put(22, f'{DIM}Model chose capability: {route_name}')
    if choice != 'needle':
        put(24, f'{PINK}EXTERNAL MODEL SELECTED. SCENARIO ENDS HERE.')
        put(25, f'{DIM}Selection only; no remote request is sent.')
        put(27, f"{ORANGE}{response['routing']['latency_ms']/1000:.1f}s actual routing · 1 on-device inference")
        time.sleep(3.2)
        return
    put(24, f'{GREEN}KEEP IT LOCAL → CALL NEEDLE AGAIN')
    put(25, f'{DIM}Same original task. Switch to device-tool schema.')
    time.sleep(2)
    base(f'{index:02d}/{len(doc["cases"]):02d}  PASS 2 — NEEDLE CALLS ITSELF', doc['before'])
    prompt(case['input'], animate=False)
    put(12, f'{PURPLE}NEEDLE (router)  →  NEEDLE (tool caller)')
    put(14, f'{DIM}Same ESP32 + weights · separate cached tool context')
    execution = response['execution']
    put(16, f'{CYAN}GENERATED → VALIDATED → EXECUTED ON DEVICE')
    for i, call in enumerate(execution['function_calls']):
        args = ', '.join(f'{k}={json.dumps(v)}' for k,v in call['arguments'].items())
        put(18+i, f'{GREEN}→ {call["name"]}({args})')
        time.sleep(.3)
    state = case['state_after']
    if case['id'] == 'status':
        details = [f"Free RAM: {state['free_internal_bytes']/1024:.0f} KiB", f"Samples: {state['samples']} · timer: {state['timer_status']}"]
    elif case['id'] == 'sampling':
        details = [f"Telemetry cadence changed to {state['sample_period_s']} seconds.", 'Periodic sampling runs independently of inference.']
    elif case['id'] == 'timer':
        details = [f"Countdown running: {state['timer_remaining_ms']/1000:.1f} seconds left.", 'An actual asynchronous ESP32 timer.']
    else:
        details = [f"Sampling: {state['sample_period_s']}s · countdown: {state['timer_status']}", 'Two local actions from one request.']
    for i, detail in enumerate(details): put(22+i, f'{WHITE}{detail}')
    put(25, f'{GREEN}✓ Local outcome verified · 0 remote model calls')
    put(27, f"{ORANGE}{response['routing']['latency_ms']/1000:.1f}s route + {execution['latency_ms']/1000:.1f}s tools · 2 real inferences")
    time.sleep(3.4)


def outro(doc):
    base('ESCALATE WHEN NEEDED. ACT LOCALLY WHEN POSSIBLE.', doc['before'])
    put(9, f'{CYAN}3 EXTERNAL CHOICES')
    put(11, f'{WHITE}Qwen 3.8 27B · GPT OSS 120B · Claude Opus')
    put(12, f'{DIM}Named selections only. Those scenarios stop there.')
    put(15, f'{GREEN}4 LOCAL REQUESTS → 8 ON-DEVICE INFERENCES')
    put(17, f'{WHITE}Status, sampling cadence, timers, combined actions.')
    put(19, f"{GREEN}✓ {doc['after']['samples']-doc['before']['samples']} samples collected · timer expiry verified")
    put(22, f'{ORANGE}Experimental routing policy; accuracy is not guaranteed.')
    put(23, f'{DIM}Curated tasks, real outputs. Latency shown throughout.')
    put(26, f'{PURPLE}github.com/iammrduncan/esp32-needle-3')
    time.sleep(5)
    put(27, f'{DIM}DEMO COMPLETE')


def main():
    doc = json.loads((ROOT/'recording.json').read_text())
    if not all(doc['verification'].values()):
        raise SystemExit('Capture contains mismatches. Review before rendering.')
    intro(doc)
    for i, case in enumerate(doc['cases'], 1): scene(doc, case, i)
    outro(doc)
    sys.stdin.readline()


if __name__ == '__main__':
    try: main()
    finally: sys.stdout.write(RESET + '\033[?25h\n')
