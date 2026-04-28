"""Generate per-segment narration using edge-tts (US English neural voice)."""
import asyncio
import json
import edge_tts
from mutagen.mp3 import MP3

VOICE = "en-US-GuyNeural"
RATE = "+0%"

SEGMENTS = [
    {"id": "intro", "text": "Introducing eAI. High-performance on-device AI for embedded systems."},
    {"id": "f1", "text": "Feature one. On-Device Inference. Run neural networks directly on microcontrollers with under 256 kilobytes of RAM."},
    {"id": "f2", "text": "Feature two. Model Quantization. Automatic INT8 and binary quantization reduces model size by 4 to 32x."},
    {"id": "f3", "text": "Feature three. Hardware Acceleration. Leverages CMSIS-NN, NPU, and DSP instructions for real-time performance."},
    {"id": "arch", "text": "Under the hood, eAI is built with C, Python, TensorFlow Lite, and CMSIS-NN. The architecture flows from Model Loader, to Quantizer, to Runtime, to Hardware Accelerator, to Inference."},
    {"id": "cta", "text": "eAI. Open source and blazing fast. Visit github dot com slash embeddedos-org slash eAI."}
]


async def generate():
    durations = {}
    audio_files = []

    for seg in SEGMENTS:
        filename = f"seg_{seg['id']}.mp3"
        communicate = edge_tts.Communicate(seg["text"], VOICE, rate=RATE)
        await communicate.save(filename)
        dur = MP3(filename).info.length
        durations[seg["id"]] = round(dur + 0.5, 1)
        audio_files.append(filename)
        print(f"  {seg['id']}: {dur:.1f}s -> padded {durations[seg['id']]}s")

    with open("durations.json", "w") as f:
        json.dump(durations, f, indent=2)

    import subprocess
    with open("concat_list.txt", "w") as f:
        for af in audio_files:
            f.write(f"file '{af}'\n")

    subprocess.run([
        "ffmpeg", "-y", "-f", "concat", "-safe", "0",
        "-i", "concat_list.txt", "-c", "copy", "narration.mp3"
    ], check=True)

    total = sum(durations.values())
    print(f"\nVoice: {VOICE}")
    print(f"Total narration: {total:.1f}s")

asyncio.run(generate())
