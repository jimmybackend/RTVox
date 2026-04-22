import math
import struct
import wave

MAGIC = b"RTVX"
VERSION = 1
SAMPLE_RATE = 16000
CHANNELS = 1
FRAME_SAMPLES = 320
BYTES_PER_FRAME = 4
PITCH_MIN_HZ = 50.0
PITCH_MAX_HZ = 400.0
FLAG_VOICED = 0x01


def clamp(value, minimum, maximum):
    return max(minimum, min(maximum, value))


def read_wav_mono16(path):
    with wave.open(path, "rb") as wf:
        if wf.getnchannels() != 1:
            raise ValueError("Input WAV must be mono.")
        if wf.getsampwidth() != 2:
            raise ValueError("Input WAV must be 16-bit PCM.")
        if wf.getframerate() != SAMPLE_RATE:
            raise ValueError("Input WAV must be 16000 Hz.")
        frames = wf.readframes(wf.getnframes())
    samples = list(struct.unpack("<{}h".format(len(frames) // 2), frames))
    return samples


def write_wav_mono16(path, samples):
    clipped = [int(clamp(s, -32768, 32767)) for s in samples]
    data = struct.pack("<{}h".format(len(clipped)), *clipped)
    with wave.open(path, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(data)


def pad_samples(samples):
    padded = list(samples)
    remainder = len(padded) % FRAME_SAMPLES
    if remainder:
        padded.extend([0] * (FRAME_SAMPLES - remainder))
    return padded


def frame_rms(frame):
    if not frame:
        return 0.0
    energy = sum((s / 32768.0) ** 2 for s in frame) / len(frame)
    return math.sqrt(max(0.0, energy))


def frame_zcr(frame):
    if len(frame) < 2:
        return 0.0
    crossings = 0
    for a, b in zip(frame[:-1], frame[1:]):
        if (a >= 0 and b < 0) or (a < 0 and b >= 0):
            crossings += 1
    return crossings / float(len(frame) - 1)


def estimate_pitch_hz(frame, sample_rate):
    min_lag = max(2, int(sample_rate / PITCH_MAX_HZ))
    max_lag = min(len(frame) - 1, int(sample_rate / PITCH_MIN_HZ))
    best_lag = 0
    best_corr = 0.0

    frame_f = [float(s) for s in frame]
    for lag in range(min_lag, max_lag + 1):
        n = len(frame_f) - lag
        if n <= 8:
            continue
        num = 0.0
        den_a = 0.0
        den_b = 0.0
        for i in range(n):
            a = frame_f[i]
            b = frame_f[i + lag]
            num += a * b
            den_a += a * a
            den_b += b * b
        if den_a <= 1e-9 or den_b <= 1e-9:
            continue
        corr = num / math.sqrt(den_a * den_b)
        if corr > best_corr:
            best_corr = corr
            best_lag = lag

    if best_lag <= 0:
        return 0.0, 0.0
    pitch_hz = sample_rate / float(best_lag)
    return pitch_hz, best_corr


def quantize_level(rms):
    return int(round(clamp(math.sqrt(clamp(rms, 0.0, 1.0)), 0.0, 1.0) * 255.0))


def dequantize_level(level_q):
    x = level_q / 255.0
    return x * x


def quantize_zcr(zcr):
    return int(round(clamp(zcr, 0.0, 1.0) * 255.0))


def dequantize_zcr(zcr_q):
    return zcr_q / 255.0


def quantize_pitch(pitch_hz):
    pitch_hz = clamp(pitch_hz, PITCH_MIN_HZ, PITCH_MAX_HZ)
    norm = (pitch_hz - PITCH_MIN_HZ) / (PITCH_MAX_HZ - PITCH_MIN_HZ)
    return int(round(norm * 255.0))


def dequantize_pitch(pitch_q):
    return PITCH_MIN_HZ + (pitch_q / 255.0) * (PITCH_MAX_HZ - PITCH_MIN_HZ)


def analyze_frame(frame):
    rms = frame_rms(frame)
    zcr = frame_zcr(frame)
    pitch_hz, corr = estimate_pitch_hz(frame, SAMPLE_RATE)
    voiced = corr > 0.30 and zcr < 0.25 and rms > 0.01
    level_q = quantize_level(rms)
    zcr_q = quantize_zcr(zcr)
    pitch_q = quantize_pitch(pitch_hz) if voiced else 0
    flags = FLAG_VOICED if voiced else 0
    return bytes([level_q, zcr_q, pitch_q, flags])


def write_rtvx(path, frames):
    header = bytearray()
    header.extend(MAGIC)
    header.append(VERSION)
    header.append(1)
    header.append(CHANNELS)
    header.append(BYTES_PER_FRAME)
    header.extend(struct.pack("<H", FRAME_SAMPLES))
    header.extend(struct.pack("<I", len(frames)))
    with open(path, "wb") as f:
        f.write(header)
        for frame in frames:
            f.write(frame)


def read_rtvx(path):
    with open(path, "rb") as f:
        data = f.read()

    if len(data) < 14:
        raise ValueError("Invalid RTVX file: too small.")
    if data[:4] != MAGIC:
        raise ValueError("Invalid RTVX file: bad magic.")
    version = data[4]
    sample_rate_code = data[5]
    channels = data[6]
    bytes_per_frame = data[7]
    frame_samples = struct.unpack("<H", data[8:10])[0]
    frame_count = struct.unpack("<I", data[10:14])[0]

    if version != VERSION:
        raise ValueError(f"Unsupported RTVX version: {version}")
    if sample_rate_code != 1:
        raise ValueError("Unsupported sample rate code.")
    if channels != 1:
        raise ValueError("Unsupported channel count.")
    if bytes_per_frame != BYTES_PER_FRAME:
        raise ValueError("Unsupported bytes per frame.")
    if frame_samples != FRAME_SAMPLES:
        raise ValueError("Unsupported frame size.")

    payload = data[14:]
    expected = frame_count * BYTES_PER_FRAME
    if len(payload) != expected:
        raise ValueError("RTVX payload size mismatch.")

    frames = [payload[i:i + BYTES_PER_FRAME] for i in range(0, len(payload), BYTES_PER_FRAME)]
    return frames


class SynthState:
    def __init__(self):
        self.phase = 0.0
        self.noise_state = 0x12345678

    def next_noise(self):
        self.noise_state = (1664525 * self.noise_state + 1013904223) & 0xFFFFFFFF
        return (((self.noise_state >> 16) & 0x7FFF) / 16384.0) - 1.0


def synthesize_frame(frame_bytes, state):
    level_q, zcr_q, pitch_q, flags = frame_bytes
    rms_hat = dequantize_level(level_q)
    zcr_hat = dequantize_zcr(zcr_q)
    voiced = (flags & FLAG_VOICED) != 0
    pitch_hz = dequantize_pitch(pitch_q) if voiced else 0.0

    amp = min(30000.0, rms_hat * 46000.0)
    out = []

    for _ in range(FRAME_SAMPLES):
        noise = state.next_noise()
        if voiced and pitch_hz > 0.0:
            tone = math.sin(2.0 * math.pi * state.phase)
            breath = noise * (0.05 + 0.10 * zcr_hat)
            sample = amp * (0.85 * tone + 0.15 * breath)
            state.phase += pitch_hz / SAMPLE_RATE
            if state.phase >= 1.0:
                state.phase -= math.floor(state.phase)
        else:
            sample = amp * noise * (0.4 + 0.6 * zcr_hat)

        out.append(int(clamp(round(sample), -32768, 32767)))

    return out
