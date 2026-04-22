# RTVox Specification v0

## Status

Experimental research prototype.

This version is meant to prove:

- open bitstream design
- encoder/decoder interoperability
- portability across C, Python, and Rust

It is not intended as a final production codec.

## Supported input

- sample rate: 16000 Hz
- channels: 1
- PCM depth: 16-bit signed little-endian
- frame size: 320 samples
- frame duration: 20 ms

## File format

### Header

All multi-byte integers are little-endian.

| Offset | Size | Field              | Value |
|-------:|-----:|--------------------|-------|
| 0      | 4    | magic              | `RTVX` |
| 4      | 1    | version            | `1` |
| 5      | 1    | sample_rate_code   | `1` => 16000 Hz |
| 6      | 1    | channels           | `1` |
| 7      | 1    | bytes_per_frame    | `4` |
| 8      | 2    | frame_samples      | `320` |
| 10     | 4    | frame_count        | number of frames |

Header size: 14 bytes.

### Frame payload

Each frame is exactly 4 bytes.

| Byte | Field    | Meaning |
|-----:|----------|---------|
| 0    | level_q  | Quantized frame energy |
| 1    | zcr_q    | Quantized zero-crossing rate |
| 2    | pitch_q  | Quantized pitch in voiced frames |
| 3    | flags    | `bit0 = voiced` |

## Encoder analysis

For every 320-sample frame:

1. Calculate RMS energy
2. Calculate zero-crossing rate
3. Estimate pitch by lag search over a voiced range
4. Mark frame as voiced or unvoiced
5. Quantize features into one frame payload

## Decoder synthesis

The decoder is deterministic and stateful.

### Voiced frames

Generate a sine-like voiced component at the decoded pitch and mix a small amount of deterministic noise.

### Unvoiced frames

Generate deterministic pseudo-random noise scaled by the decoded level.

## Current limitations

- not transparent audio coding
- not production voice quality
- no packet-loss concealment yet
- no VAD/DTX yet
- no RTP mapping yet
- voice-only prototype
