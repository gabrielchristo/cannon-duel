#!/usr/bin/env python3
"""Sintetiza efeitos sonoros do Cannon Duel (tiro e explosão).

Usa apenas a stdlib (wave + math). Gera WAV intermediários e, se ffmpeg
estiver disponível, converte automaticamente para OGG (libvorbis).

Dependências:
    Python 3.8+ (stdlib)
    ffmpeg (opcional, para .ogg)

Uso:
    python3 tools/gen_sounds.py
    python3 tools/gen_sounds.py --output-dir assets/sounds --skip-ogg
"""

from __future__ import annotations

import argparse
import math
import random
import shutil
import struct
import subprocess
import sys
import wave
from pathlib import Path
from typing import Callable

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_OUT = SCRIPT_DIR.parent / "assets" / "sounds"
SAMPLE_RATE = 44_100


def write_wav(path: Path, samples: list[float], sample_rate: int = SAMPLE_RATE) -> None:
    """Grava amostras mono float [-1, 1] como WAV PCM 16-bit."""
    clipped = [max(-1.0, min(1.0, s)) for s in samples]
    frames = b"".join(struct.pack("<h", int(s * 32767)) for s in clipped)
    with wave.open(str(path), "w") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(sample_rate)
        f.writeframes(frames)


def envelope_exp_decay(count: int, tau_samples: float) -> list[float]:
    return [math.exp(-i / tau_samples) for i in range(count)]


def white_noise(count: int, seed: int = 0) -> list[float]:
    rng = random.Random(seed)
    return [rng.uniform(-1.0, 1.0) for _ in range(count)]


def low_pass(samples: list[float], alpha: float) -> list[float]:
    """Filtro passa-baixa de 1 polo; alpha menor = mais grave."""
    out: list[float] = []
    prev = 0.0
    for sample in samples:
        prev = prev + alpha * (sample - prev)
        out.append(prev)
    return out


def mix(*tracks: list[float]) -> list[float]:
    length = max(len(t) for t in tracks)
    out = [0.0] * length
    for track in tracks:
        for i, sample in enumerate(track):
            out[i] += sample
    return out


def sine_sweep(count: int, freq_start: float, freq_end: float, sample_rate: int = SAMPLE_RATE) -> list[float]:
    out: list[float] = []
    for i in range(count):
        t = i / sample_rate
        freq = freq_start + (freq_end - freq_start) * (i / count)
        out.append(math.sin(2 * math.pi * freq * t))
    return out


def normalize(samples: list[float], peak_target: float) -> list[float]:
    peak = max(abs(s) for s in samples) or 1.0
    return [s / peak * peak_target for s in samples]


def make_fire() -> list[float]:
    """Tiro: thump grave (sweep descendente) + estalo de ruído filtrado."""
    duration = 0.35
    count = int(SAMPLE_RATE * duration)

    thump = sine_sweep(count, 180, 45)
    thump = [s * e for s, e in zip(thump, envelope_exp_decay(count, SAMPLE_RATE * 0.05))]

    crack_count = int(SAMPLE_RATE * 0.06)
    crack = low_pass(white_noise(crack_count, seed=1), 0.5)
    crack = [s * e * 1.4 for s, e in zip(crack, envelope_exp_decay(crack_count, SAMPLE_RATE * 0.008))]
    crack = crack + [0.0] * (count - crack_count)

    return normalize(mix(thump, crack), 0.9)


def make_explosion() -> list[float]:
    """Explosão: ruído grave longo + sub-bass + estalo inicial."""
    duration = 1.1
    count = int(SAMPLE_RATE * duration)

    filtered = low_pass(white_noise(count, seed=2), 0.12)
    body = [s * e for s, e in zip(filtered, envelope_exp_decay(count, SAMPLE_RATE * 0.35))]

    sub = sine_sweep(count, 90, 30)
    sub = [s * e * 1.2 for s, e in zip(sub, envelope_exp_decay(count, SAMPLE_RATE * 0.4))]

    crack_count = int(SAMPLE_RATE * 0.05)
    crack = white_noise(crack_count, seed=3)
    crack = [s * e * 1.5 for s, e in zip(crack, envelope_exp_decay(crack_count, SAMPLE_RATE * 0.01))]
    crack = crack + [0.0] * (count - crack_count)

    return normalize(mix(body, sub, crack), 0.95)


def convert_to_ogg(wav_path: Path, ogg_path: Path) -> bool:
    """Converte WAV → OGG via ffmpeg. Retorna True se bem-sucedido."""
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        return False
    result = subprocess.run(
        [ffmpeg, "-y", "-i", str(wav_path), "-c:a", "libvorbis", "-qscale:a", "5", str(ogg_path)],
        capture_output=True,
        text=True,
    )
    return result.returncode == 0


SOUND_DEFS: tuple[tuple[str, Callable[[], list[float]]], ...] = (
    ("fire", make_fire),
    ("explosion", make_explosion),
)


def generate_sounds(output_dir: Path, *, skip_ogg: bool) -> None:
    """Gera WAV (e opcionalmente OGG) para cada efeito."""
    output_dir.mkdir(parents=True, exist_ok=True)
    has_ffmpeg = shutil.which("ffmpeg") is not None

    for name, maker in SOUND_DEFS:
        wav_path = output_dir / f"{name}_raw.wav"
        ogg_path = output_dir / f"{name}.ogg"
        write_wav(wav_path, maker())
        print(f"  {wav_path.name}")

        if skip_ogg:
            continue

        if convert_to_ogg(wav_path, ogg_path):
            print(f"  {ogg_path.name}")
        elif not has_ffmpeg:
            print(
                f"  (ffmpeg não encontrado — converta manualmente: "
                f"ffmpeg -y -i {wav_path} -c:a libvorbis -qscale:a 5 {ogg_path})",
                file=sys.stderr,
            )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Sintetiza efeitos sonoros do Cannon Duel.")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=DEFAULT_OUT,
        help=f"Diretório de saída (padrão: {DEFAULT_OUT})",
    )
    parser.add_argument(
        "--skip-ogg",
        action="store_true",
        help="Gera apenas os arquivos WAV intermediários",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    output_dir = args.output_dir.resolve()
    print(f"Gerando sons em {output_dir} …")
    generate_sounds(output_dir, skip_ogg=args.skip_ogg)
    print("Concluído.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
