#!/usr/bin/env python3
"""Render the Needle 3 autoresearch film and poster entirely from code.

The motion design is drawn frame-by-frame with Pillow. FFmpeg is used only as
the codec: no timeline editor, motion template, or hand-authored frame asset is
involved. The poster combines the exact, code-rendered metrics with the
text-free photographic background generated for this campaign.
"""

from __future__ import annotations

import argparse
import math
import shutil
import subprocess
from pathlib import Path

from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageFont


ROOT = Path(__file__).resolve().parent
WIDTH, HEIGHT = 1280, 720
FPS = 24
DURATION = 18.0

CARBON = (17, 19, 21)
PAPER = (244, 242, 234)
WHITE = (255, 255, 255)
CORAL = (255, 85, 61)
CORAL_DEEP = (200, 50, 30)
GRAPHITE = (85, 91, 88)
ALLOY = (174, 179, 176)
FOG = (216, 220, 217)

MILESTONES = [
    (1, 1.2217, "FIRST MEASURED BASELINE"),
    (2, 2.4417, "FP16 WEIGHT STAGING"),
    (31, 3.8633, "DUAL-CORE COVERAGE"),
    (51, 4.1800, "HOT WEIGHTS IN PSRAM"),
    (137, 4.7350, "HAND-WRITTEN TIE728 SIMD"),
    (147, 4.8817, "GRAMMAR FIRST-BYTE INDEX"),
    (431, 5.3033, "BUNDLE 5"),
    (646, 5.8083, "KV STAGING"),
    (692, 5.9617, "WIDE LANE MIX"),
    (872, 6.1550, "INTERLEAVED QK"),
    (941, 6.2200, "W3 PREFIX ELIMINATION"),
]


def font_path(*candidates: str) -> str:
    roots = (Path("/usr/share/fonts"), Path("/usr/local/share/fonts"))
    for root in roots:
        for candidate in candidates:
            hits = sorted(root.rglob(candidate)) if root.exists() else []
            if hits:
                return str(hits[0])
    raise FileNotFoundError(f"No font found for {candidates}")


DISPLAY_FONT = font_path("DejaVuSansCondensed-Bold.ttf", "NotoSans-CondensedBlack.ttf", "NotoSans-Bold.ttf")
TEXT_FONT = font_path("NotoSans-Regular.ttf", "DejaVuSans.ttf")
MONO_FONT = font_path("DejaVuSansMono.ttf", "NotoSansMono-Regular.ttf")


def fnt(size: int, kind: str = "display") -> ImageFont.FreeTypeFont:
    path = {"display": DISPLAY_FONT, "text": TEXT_FONT, "mono": MONO_FONT}[kind]
    return ImageFont.truetype(path, size=size)


def clamp(value: float, low: float = 0.0, high: float = 1.0) -> float:
    return max(low, min(high, value))


def smooth(value: float) -> float:
    value = clamp(value)
    return value * value * (3.0 - 2.0 * value)


def out_cubic(value: float) -> float:
    value = clamp(value)
    return 1.0 - (1.0 - value) ** 3


def scene_progress(now: float, start: float, end: float) -> float:
    return clamp((now - start) / (end - start))


def text(draw: ImageDraw.ImageDraw, xy: tuple[float, float], value: str, size: int,
         fill=PAPER, kind: str = "display", anchor: str | None = None,
         spacing: int = 4, stroke_width: int = 0, stroke_fill=None) -> None:
    draw.multiline_text(
        xy, value, font=fnt(size, kind), fill=fill, anchor=anchor,
        spacing=spacing, stroke_width=stroke_width, stroke_fill=stroke_fill,
    )


def fit_text(draw: ImageDraw.ImageDraw, value: str, max_width: int, start_size: int,
             kind: str = "display") -> ImageFont.FreeTypeFont:
    size = start_size
    while size > 12:
        font = fnt(size, kind)
        if draw.textbbox((0, 0), value, font=font)[2] <= max_width:
            return font
        size -= 2
    return fnt(size, kind)


def draw_header(draw: ImageDraw.ImageDraw, now: float) -> None:
    draw.rectangle((0, 0, WIDTH, 66), fill=CARBON)
    draw.line((42, 65, WIDTH - 42, 65), fill=(56, 59, 59), width=1)
    draw.ellipse((43, 22, 61, 40), outline=CORAL, width=3)
    draw.ellipse((55, 22, 73, 40), outline=CORAL, width=3)
    text(draw, (87, 19), "NEEDLE 3 / ESP32-S3", 18, kind="mono")
    right = "AUTORESEARCH // 943 LOGGED RUNS"
    text(draw, (WIDTH - 43, 20), right, 16, fill=ALLOY, kind="mono", anchor="ra")
    pulse = int(120 + 100 * (0.5 + 0.5 * math.sin(now * 5.0)))
    draw.ellipse((WIDTH - 383, 26, WIDTH - 373, 36), fill=(255, 85, 61, pulse))


def draw_grid(draw: ImageDraw.ImageDraw, offset: float = 0.0, light: bool = False) -> None:
    color = (224, 222, 214) if light else (31, 35, 36)
    for x in range(-40, WIDTH + 80, 40):
        draw.line((x + offset % 40, 66, x + offset % 40, HEIGHT), fill=color, width=1)
    for y in range(80, HEIGHT + 40, 40):
        draw.line((0, y, WIDTH, y), fill=color, width=1)


def infinity_points(cx: float, cy: float, rx: float, ry: float, phase: float = 0.0,
                    count: int = 180) -> list[tuple[float, float]]:
    points = []
    for i in range(count):
        a = phase + 2.0 * math.pi * i / (count - 1)
        den = 1.0 + math.sin(a) ** 2
        points.append((cx + rx * math.cos(a) / den, cy + ry * math.sin(a) * math.cos(a) / den))
    return points


def draw_infinity(draw: ImageDraw.ImageDraw, now: float, cx: float, cy: float,
                  rx: float, ry: float, alpha: float = 1.0) -> None:
    base = infinity_points(cx, cy, rx, ry)
    glow = tuple(int(c * alpha) for c in CORAL)
    draw.line(base, fill=CORAL_DEEP, width=max(2, int(12 * alpha)), joint="curve")
    draw.line(base, fill=glow, width=max(1, int(4 * alpha)), joint="curve")
    dot_phase = (now * 0.34) % 1.0
    idx = int(dot_phase * (len(base) - 1))
    px, py = base[idx]
    r = 8 + 4 * math.sin(now * 7.0)
    draw.ellipse((px - r, py - r, px + r, py + r), fill=CORAL)


def draw_chip(draw: ImageDraw.ImageDraw, now: float, center=(935, 384), scale=1.0) -> None:
    cx, cy = center
    w, h = 250 * scale, 330 * scale
    x0, y0, x1, y1 = cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2
    draw.rounded_rectangle((x0, y0, x1, y1), radius=int(18 * scale), fill=(25, 29, 29), outline=ALLOY, width=2)
    for side in (-1, 1):
        x = x0 - 18 * scale if side < 0 else x1 + 18 * scale
        for i in range(12):
            yy = y0 + 28 * scale + i * 24 * scale
            draw.line((x0 if side < 0 else x1, yy, x, yy), fill=FOG, width=max(1, int(3 * scale)))
    die = (cx - 68 * scale, cy - 86 * scale, cx + 68 * scale, cy + 50 * scale)
    draw.rounded_rectangle(die, radius=int(8 * scale), fill=(47, 51, 50), outline=FOG, width=2)
    text(draw, (cx, cy - 37 * scale), "S3", max(16, int(42 * scale)), fill=PAPER, anchor="mm")
    text(draw, (cx, cy + 24 * scale), "240 MHz", max(10, int(15 * scale)), fill=ALLOY, kind="mono", anchor="mm")
    for i in range(4):
        angle = now * (0.8 + i * 0.08) + i * 1.5
        px = cx + math.cos(angle) * (42 + i * 15) * scale
        py = cy + 102 * scale + math.sin(angle) * 11 * scale
        r = (3 + i) * scale
        draw.ellipse((px - r, py - r, px + r, py + r), fill=CORAL)


def metric_at(progress: float) -> float:
    pos = clamp(progress) * (len(MILESTONES) - 1)
    idx = min(len(MILESTONES) - 2, int(pos))
    local = smooth(pos - idx)
    return MILESTONES[idx][1] * (1 - local) + MILESTONES[idx + 1][1] * local


def draw_timeline(draw: ImageDraw.ImageDraw, progress: float, now: float) -> None:
    left, right = 76, WIDTH - 76
    top, bottom = 170, 545
    draw.line((left, bottom, right, bottom), fill=GRAPHITE, width=2)
    draw.line((left, top, left, bottom), fill=GRAPHITE, width=2)
    for value in (1, 2, 3, 4, 5, 6):
        y = bottom - (value - 1) / 5.4 * (bottom - top)
        draw.line((left, y, right, y), fill=(44, 48, 48), width=1)
        text(draw, (left - 17, y), str(value), 13, fill=ALLOY, kind="mono", anchor="rm")

    points = []
    for i, (_, value, _) in enumerate(MILESTONES):
        x = left + i / (len(MILESTONES) - 1) * (right - left)
        y = bottom - (value - 1) / 5.4 * (bottom - top)
        points.append((x, y))
    limit = progress * (len(points) - 1)
    visible = min(len(points) - 1, int(limit))
    partial = limit - visible
    shown = points[:visible + 1]
    if visible < len(points) - 1:
        ax, ay = points[visible]
        bx, by = points[visible + 1]
        shown.append((ax + (bx - ax) * partial, ay + (by - ay) * partial))
    if len(shown) > 1:
        draw.line(shown, fill=CORAL_DEEP, width=13, joint="curve")
        draw.line(shown, fill=CORAL, width=5, joint="curve")
    for i, (x, y) in enumerate(points):
        if i <= limit + 0.02:
            r = 7 if i not in (0, len(points) - 1) else 10
            draw.ellipse((x - r, y - r, x + r, y + r), fill=CORAL, outline=PAPER, width=2)

    current = metric_at(progress)
    text(draw, (left, 96), "THE CLIMB", 18, fill=CORAL, kind="mono")
    text(draw, (left, 116), f"{current:0.2f}", 78, fill=PAPER)
    text(draw, (left + 225, 169), "TOKENS / SECOND", 18, fill=ALLOY, kind="mono")
    run_idx = min(len(MILESTONES) - 1, int(round(progress * (len(MILESTONES) - 1))))
    run, _, label = MILESTONES[run_idx]
    text(draw, (right, 116), f"RUN {run:03d}  /  {label}", 17, fill=PAPER, kind="mono", anchor="ra")
    scan_x = left + progress * (right - left)
    draw.line((scan_x, top - 5, scan_x, bottom + 25), fill=(255, 85, 61), width=1)
    text(draw, (left, 584), "1.22", 16, fill=ALLOY, kind="mono")
    text(draw, (right, 584), "6.22", 16, fill=CORAL, kind="mono", anchor="ra")
    text(draw, (left, 624), "Every point measured on real ESP32-S3 hardware.", 21, fill=PAPER, kind="text")


def draw_opening(frame: Image.Image, now: float) -> None:
    draw = ImageDraw.Draw(frame)
    draw_grid(draw, now * 12)
    p = out_cubic(scene_progress(now, 0.0, 1.0))
    x = 65 + (1 - p) * 85
    text(draw, (x, 128), "THE FIRST\nREADING", 76, fill=PAPER)
    text(draw, (x + 5, 318), "RUN 001 / SEPTEMBER 18, 2026", 17, fill=CORAL, kind="mono")
    text(draw, (x, 362), "1.22", 118, fill=PAPER)
    text(draw, (x + 320, 438), "TOK/S", 25, fill=ALLOY, kind="mono")
    text(draw, (x + 4, 510), "One tiny chip. Roughly one token each second.", 24, fill=FOG, kind="text")
    draw_infinity(draw, now, 986, 355, 250, 240, alpha=0.9)
    draw_chip(draw, now, center=(986, 365), scale=0.78)
    if now > 2.15:
        a = scene_progress(now, 2.15, 3.2)
        draw.rectangle((0, HEIGHT - int(170 * smooth(a)), WIDTH, HEIGHT), fill=CORAL)
        text(draw, (WIDTH / 2, HEIGHT - 86), "THEN THE CURVE BROKE.", 49, fill=CARBON, anchor="mm")


def draw_first_breakthrough(frame: Image.Image, now: float) -> None:
    draw = ImageDraw.Draw(frame)
    local = now - 3.0
    draw.rectangle((0, 66, WIDTH, HEIGHT), fill=PAPER)
    draw_grid(draw, local * 16, light=True)
    p = out_cubic(scene_progress(local, 0.0, 1.6))
    before = 1.2217
    value = before + (2.4417 - before) * p
    text(draw, (68, 118), "BREAKTHROUGH 001", 18, fill=CORAL_DEEP, kind="mono")
    text(draw, (68, 155), "MOVE THE CONVERSION.\nKEEP THE MATH.", 63, fill=CARBON)
    text(draw, (68, 344), f"{value:0.2f}", 126, fill=CARBON)
    text(draw, (415, 427), "TOK/S", 24, fill=GRAPHITE, kind="mono")
    if p > 0.75:
        text(draw, (70, 520), "+99.9%", 45, fill=CORAL_DEEP)
        text(draw, (277, 538), "IN ONE VERIFIED CHANGE", 18, fill=GRAPHITE, kind="mono")
    draw_chip(draw, local, center=(986, 390), scale=0.84)
    draw_infinity(draw, local, 986, 390, 255, 225, alpha=1.0)
    text(draw, (69, 646), "FP16 → FP32 WEIGHT STAGING  /  BYTE-EXACT OUTPUT", 17, fill=GRAPHITE, kind="mono")


def draw_breakthroughs(frame: Image.Image, now: float) -> None:
    draw = ImageDraw.Draw(frame)
    draw.rectangle((0, 66, WIDTH, HEIGHT), fill=CARBON)
    draw_grid(draw, now * 18)
    local = now - 10.6
    cards = [
        (31, "DUAL-CORE\nSCHEDULING", "3.86 TOK/S"),
        (137, "HAND-WRITTEN\nTIE728 SIMD", "4.74 TOK/S"),
        (147, "GRAMMAR\nFIRST-BYTE INDEX", "4.88 TOK/S"),
        (431, "BUNDLE 5", "5.30 TOK/S"),
        (646, "KV STAGING", "5.81 TOK/S"),
        (692, "WIDE\nLANE MIX", "5.96 TOK/S"),
        (872, "INTERLEAVED QK", "6.16 TOK/S"),
        (941, "W3 PREFIX\nELIMINATION", "6.22 TOK/S"),
    ]
    idx = min(len(cards) - 1, int(max(0.0, local) / 0.42))
    within = (max(0.0, local) / 0.42) % 1.0
    run, label, speed = cards[idx]
    jitter = (1.0 - out_cubic(min(1.0, within * 2.4))) * 28
    x = 68 + jitter
    text(draw, (x, 111), f"RUN {run:03d}", 20, fill=CORAL, kind="mono")
    font = fit_text(draw, label.split("\n")[0], 1080, 90)
    draw.multiline_text((x, 162), label, font=font, fill=PAPER, spacing=0)
    text(draw, (x, 410), speed, 61, fill=CORAL)
    draw.rectangle((68, 530, WIDTH - 68, 534), fill=(44, 48, 48))
    card_progress = (idx + smooth(within)) / len(cards)
    draw.rectangle((68, 530, 68 + (WIDTH - 136) * card_progress, 534), fill=CORAL)
    text(draw, (68, 565), "943 runs means 942 chances to be wrong — and measure it.", 27, fill=FOG, kind="text")
    text(draw, (68, 635), f"{idx + 1:02d} / {len(cards):02d}", 16, fill=ALLOY, kind="mono")
    for i in range(24):
        px = WIDTH - 140 - (i * 41 + int(now * 115)) % 420
        py = 135 + ((i * 73) % 370)
        radius = 2 + (i % 4)
        draw.ellipse((px - radius, py - radius, px + radius, py + radius), fill=CORAL if i % 3 == 0 else GRAPHITE)


def draw_final(frame: Image.Image, now: float) -> None:
    draw = ImageDraw.Draw(frame)
    local = now - 14.0
    reveal = out_cubic(scene_progress(local, 0.0, 0.8))
    draw.rectangle((0, 66, WIDTH, HEIGHT), fill=CARBON)
    draw_grid(draw, -now * 9)
    draw_infinity(draw, now, 1007, 356, 300, 260, alpha=1.0)
    draw_chip(draw, now, center=(1007, 371), scale=0.82)
    left = 65 + (1.0 - reveal) * 70
    text(draw, (left, 108), "FASTEST VERIFIED / NOT YET PROMOTED", 17, fill=CORAL, kind="mono")
    text(draw, (left, 150), "6.22", 145, fill=PAPER)
    text(draw, (left + 406, 255), "TOK/S", 27, fill=ALLOY, kind="mono")
    text(draw, (left + 5, 337), "5.09×", 55, fill=CORAL)
    text(draw, (left + 191, 366), "THE MEASURED BASELINE", 18, fill=FOG, kind="mono")
    text(draw, (left + 5, 432), "818 ms → 161 ms / token", 25, fill=PAPER, kind="mono")
    text(draw, (left + 5, 489), "943", 50, fill=PAPER)
    text(draw, (left + 154, 518), "LOGGED HARDWARE RUNS", 18, fill=ALLOY, kind="mono")
    text(draw, (left + 5, 585), "A small chip. A stubborn loop. A result nobody will forget.", 22, fill=FOG, kind="text")
    if local > 2.6:
        p = smooth(scene_progress(local, 2.6, 3.4))
        y = HEIGHT - 56 + (1 - p) * 70
        draw.rectangle((0, y - 21, WIDTH, HEIGHT), fill=CORAL)
        text(draw, (WIDTH / 2, y + 4), "KEEP BUILDING. KEEP MEASURING.", 24, fill=CARBON, anchor="mm")


def render_frame(index: int) -> Image.Image:
    now = index / FPS
    frame = Image.new("RGB", (WIDTH, HEIGHT), CARBON)
    if now < 3.2:
        draw_opening(frame, now)
    elif now < 6.0:
        draw_first_breakthrough(frame, now)
    elif now < 10.6:
        draw = ImageDraw.Draw(frame)
        draw.rectangle((0, 66, WIDTH, HEIGHT), fill=CARBON)
        draw_grid(draw, now * 10)
        draw_timeline(draw, smooth(scene_progress(now, 6.0, 10.4)), now)
    elif now < 14.0:
        draw_breakthroughs(frame, now)
    else:
        draw_final(frame, now)
    draw_header(ImageDraw.Draw(frame), now)
    return frame


def ffmpeg_executable() -> str:
    system = shutil.which("ffmpeg")
    if system:
        return system
    try:
        import imageio_ffmpeg
        return imageio_ffmpeg.get_ffmpeg_exe()
    except ImportError as exc:
        raise RuntimeError("Install imageio-ffmpeg or put ffmpeg on PATH") from exc


def render_video(output: Path) -> None:
    ffmpeg = ffmpeg_executable()
    cmd = [
        ffmpeg, "-y", "-loglevel", "warning",
        "-f", "rawvideo", "-pixel_format", "rgb24",
        "-video_size", f"{WIDTH}x{HEIGHT}", "-framerate", str(FPS), "-i", "-",
        "-an", "-c:v", "libx264", "-preset", "slow", "-crf", "19",
        "-pix_fmt", "yuv420p", "-movflags", "+faststart", str(output),
    ]
    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE)
    assert proc.stdin is not None
    for index in range(round(DURATION * FPS)):
        proc.stdin.write(render_frame(index).tobytes())
    proc.stdin.close()
    if proc.wait() != 0:
        raise RuntimeError("FFmpeg failed while encoding MP4")


def render_gif(video: Path, output: Path) -> None:
    ffmpeg = ffmpeg_executable()
    palette = output.with_suffix(".palette.png")
    filters = "fps=10,scale=960:-1:flags=lanczos"
    subprocess.run([
        ffmpeg, "-y", "-loglevel", "warning", "-i", str(video),
        "-vf", f"{filters},palettegen=max_colors=128:stats_mode=diff",
        "-frames:v", "1", str(palette),
    ], check=True)
    subprocess.run([
        ffmpeg, "-y", "-loglevel", "warning", "-i", str(video), "-i", str(palette),
        "-lavfi", f"{filters}[x];[x][1:v]paletteuse=dither=bayer:bayer_scale=4",
        "-loop", "0", str(output),
    ], check=True)
    palette.unlink(missing_ok=True)


def poster_background() -> Image.Image:
    source = Image.open(ROOT / "needle-progress-background.png").convert("RGB")
    target_ratio = 16 / 9
    ratio = source.width / source.height
    if ratio > target_ratio:
        new_width = int(source.height * target_ratio)
        left = (source.width - new_width) // 2
        source = source.crop((left, 0, left + new_width, source.height))
    else:
        new_height = int(source.width / target_ratio)
        top = (source.height - new_height) // 2
        source = source.crop((0, top, source.width, top + new_height))
    source = source.resize((2400, 1350), Image.Resampling.LANCZOS)
    source = ImageEnhance.Contrast(source).enhance(1.08)
    source = ImageEnhance.Color(source).enhance(0.84)
    return source


def render_poster(output: Path) -> None:
    image = poster_background()
    overlay = Image.new("RGBA", image.size, (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    for x in range(0, 1500):
        alpha = int(248 * (1.0 - smooth(max(0.0, (x - 860) / 640))))
        od.line((x, 0, x, image.height), fill=(17, 19, 21, alpha))
    od.rectangle((0, 0, image.width, 112), fill=(17, 19, 21, 240))
    od.line((78, 111, image.width - 78, 111), fill=(90, 94, 93, 220), width=2)
    image = Image.alpha_composite(image.convert("RGBA"), overlay)
    draw = ImageDraw.Draw(image)

    draw.ellipse((79, 34, 113, 68), outline=CORAL, width=5)
    draw.ellipse((102, 34, 136, 68), outline=CORAL, width=5)
    text(draw, (159, 31), "NEEDLE 3 / ESP32-S3", 31, kind="mono")
    text(draw, (image.width - 78, 34), "AUTORESEARCH / FINAL FIELD REPORT", 26, fill=ALLOY, kind="mono", anchor="ra")

    text(draw, (82, 176), "FROM ~ONE TOKEN\nTO SIX.", 108, fill=PAPER)
    text(draw, (87, 467), "1.22", 176, fill=ALLOY)
    draw.line((468, 570, 608, 570), fill=CORAL, width=15)
    draw.polygon(((608, 538), (665, 570), (608, 602)), fill=CORAL)
    text(draw, (694, 467), "6.22", 176, fill=PAPER)
    text(draw, (1051, 606), "TOK/S", 31, fill=ALLOY, kind="mono")
    text(draw, (90, 696), "5.09× FASTER", 60, fill=CORAL)
    text(draw, (92, 784), "818 ms → 161 ms per token", 30, fill=PAPER, kind="mono")

    draw.rectangle((86, 866, 1190, 869), fill=GRAPHITE)
    cards = [
        ("943", "LOGGED RUNS"),
        ("23/23", "HOST EXACT"),
        ("6.53", "PREFILL TOK/S"),
    ]
    for i, (value, label) in enumerate(cards):
        x = 87 + i * 365
        text(draw, (x, 907), value, 55, fill=PAPER)
        text(draw, (x, 975), label, 20, fill=ALLOY, kind="mono")

    text(draw, (88, 1058), "BREAKTHROUGHS", 21, fill=CORAL, kind="mono")
    text(draw, (88, 1100), "FP32 staging  /  dual core  /  TIE728 SIMD\ngrammar index  /  KV staging  /  wide delivery", 27, fill=PAPER, kind="text", spacing=16)
    text(draw, (88, 1278), "FASTEST VERIFIED · QUALITY FROZEN · CURRENT RESULT NOT YET PROMOTED", 22, fill=ALLOY, kind="mono")
    image.convert("RGB").save(output, quality=95)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--video", type=Path, default=ROOT / "needle-progress.mp4")
    parser.add_argument("--gif", type=Path, default=ROOT / "needle-progress.gif")
    parser.add_argument("--poster", type=Path, default=ROOT / "needle-progress-poster.png")
    parser.add_argument("--poster-only", action="store_true")
    args = parser.parse_args()
    render_poster(args.poster)
    if not args.poster_only:
        render_video(args.video)
        render_gif(args.video, args.gif)


if __name__ == "__main__":
    main()
