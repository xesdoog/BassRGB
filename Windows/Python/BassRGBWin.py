import matplotlib.pyplot as plt
import numpy as np
import subprocess
import time

from openrgb import OpenRGBClient
from openrgb.utils import RGBColor
from scipy.fftpack import fft


SAMPLE_RATE = 44100
CHUNK_SIZE = 1024
BASS_RANGE = (10, 300)

ffmpeg = [
    "ffmpeg",
    "-f",
    "dshow",
    "-i",
    "audio=CABLE Output (VB-Audio Virtual Cable)",
    "-ac",
    "1",
    "-ar",
    str(SAMPLE_RATE),
    "-f",
    "wav",
    "-bufsize",
    "512k",
    "pipe:1",
]

process = subprocess.Popen(
    ffmpeg, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, bufsize=CHUNK_SIZE * 10
)

if process.poll() is not None:
    print("FFmpeg failed to start.")
    exit()

client = OpenRGBClient()

# DRAM test
devices = [device for device in client.devices if "DRAM" in device.name]

if not devices:
    print("No devices found.")
    exit()


def get_bass_freqs():
    freqs = np.fft.rfftfreq(CHUNK_SIZE, d=1.0 / SAMPLE_RATE)
    return np.where((freqs >= BASS_RANGE[0]) & (freqs <= BASS_RANGE[1]))[0]


# debug
# def check_audio():
#     print("Checking audio input...")
#     while True:
#         raw_audio = process.stdout.read(CHUNK_SIZE * 2)
#         if not raw_audio:
#             print("No audio data received.")
#         else:
#             print(f"Captured {len(raw_audio)} bytes.")
#             break


def bass_rgb():
    bass_history = []
    noise_floor = None
    debug_plot = False
    silent_threshold = 10000000  # weak signals
    alpha = 0.7  # smoothing
    bass_frequencies = get_bass_freqs()

    if debug_plot:
        plt.ion()
        _, ax = plt.subplots()
        (line,) = ax.plot([], [], lw=2)
        ax.set_xlim(0, len(bass_frequencies))
        ax.set_ylim(0, 1e7)
        ax.set_xlabel("Frequency Index")
        ax.set_ylabel("Amplitude")

    while True:
        raw_audio = process.stdout.read(CHUNK_SIZE * 2)

        if not raw_audio:
            print("No audio data received.")
            time.sleep(0.1)
            continue

        audio_data = np.frombuffer(raw_audio, dtype=np.int16)

        if len(audio_data) == 0:
            print("Empty audio buffer.")
            continue

        if len(audio_data) < CHUNK_SIZE:
            print("Incomplete audio chunk.")
            continue

        fft_data = np.abs(fft(audio_data)[: len(audio_data) // 2])
        bass_values = fft_data[bass_frequencies]
        bass_intensity = np.sum(bass_values)

        if noise_floor is None:
            noise_floor = bass_intensity
        else:
            noise_floor = (noise_floor * 0.98) + (bass_intensity * 0.02)

        if bass_intensity < silent_threshold:
            adjusted_bass = 0
        else:
            adjusted_bass = max(bass_intensity - noise_floor, 0)

        if bass_history:
            adjusted_bass = (alpha * adjusted_bass) + ((1 - alpha) * bass_history[-1])

        bass_history.append(adjusted_bass)

        if len(bass_history) > 50:
            bass_history.pop(0)

        max_seen = max(bass_history) if bass_history and max(bass_history) > 0 else 1
        scaled_intensity = np.sqrt(adjusted_bass) / np.sqrt(max_seen) * 255
        if np.isnan(scaled_intensity) or np.isinf(scaled_intensity):
            scaled_intensity = 0

        intensity = max(0, min(int(scaled_intensity), 255))

        # print(
        #     f"Bass Intensity: {bass_intensity:.2f}, Noise Floor: {noise_floor:.2f}, Adjusted: {adjusted_bass:.2f}, Final (RGB): {intensity}",
        #     end="\r",
        #     flush=True,
        # )

        if debug_plot:
            line.set_ydata(bass_values)
            line.set_xdata(range(len(bass_values)))
            ax.set_ylim(0, max(bass_values) * 1.2)

        plt.pause(0.01)

        # # set all channels based on bass intensity
        # color = RGBColor(intensity, intensity, intensity)

        # change colors based on bass intensity
        if intensity < 85:
            color = RGBColor(intensity, 0, 255)
        elif intensity < 170:
            color = RGBColor(0, intensity, 255 - intensity)
        else:
            color = RGBColor(255, intensity, 0)

        for device in devices:
            device.set_color(color, fast=True)


if __name__ == "__main__":
    bass_rgb()
