# RTVox

RTVox is an open-source experimental real-time voice codec focused on intelligibility, low bitrate operation, packet-loss resilience, and cross-language interoperability.

This repository includes the same proof-of-concept encoder/decoder implemented in:

- C
- Python
- Rust

## What is included

This ZIP contains a small interoperable prototype, not a production-ready codec.

The current bitstream is intentionally simple so it can be implemented in multiple languages and understood easily:

- 16 kHz
- mono
- 16-bit PCM WAV input
- 20 ms frames (320 samples)
- 4 bytes per frame
- deterministic decoder synthesis
- same container format in C, Python, and Rust

Each frame stores:

- quantized energy
- quantized zero-crossing rate
- quantized pitch
- voiced/unvoiced flag

The decoder rebuilds a voice-like signal from those parameters.

## Important note

RTVox v0 is a research prototype.
It is useful for:

- testing repository structure
- validating cross-language bitstream compatibility
- experimenting with codec ideas
- starting an open GitHub project

It is not suitable yet for production VoIP calls.

## Repository layout

```text
rtvox/
├── README.md
├── LICENSE
├── CONTRIBUTING.md
├── docs/
│   └── spec-v0.md
├── c/
│   ├── CMakeLists.txt
│   ├── include/
│   │   └── rtvox.h
│   └── src/
│       ├── rtvox_common.c
│       ├── rtvoxenc.c
│       └── rtvoxdec.c
├── python/
│   ├── common.py
│   ├── encoder.py
│   └── decoder.py
├── rust/
│   ├── Cargo.toml
│   └── src/
│       ├── common.rs
│       ├── encoder.rs
│       ├── decoder.rs
│       └── main.rs
└── examples/
    └── test_input.wav
```

## Bitstream summary

Header:

- magic: `RTVX`
- version: `1`
- sample rate code: `1` = 16000 Hz
- channels: `1`
- bytes per frame: `4`
- frame samples: `320`
- frame count: `u32 little-endian`

Frame payload (4 bytes per frame):

- byte 0: `level_q`
- byte 1: `zcr_q`
- byte 2: `pitch_q`
- byte 3: flags (`bit0 = voiced`)

## Build and run

### Python

Encode:

```bash
python3 python/encoder.py input.wav output.rtvx
```

Decode:

```bash
python3 python/decoder.py output.rtvx reconstructed.wav
```

### C

Build:

```bash
cd c
cmake -S . -B build
cmake --build build
```

Encode:

```bash
./build/rtvoxenc input.wav output.rtvx
```

Decode:

```bash
./build/rtvoxdec output.rtvx reconstructed.wav
```

### Rust

Build:

```bash
cd rust
cargo build --release
```

Encode:

```bash
cargo run --release -- enc input.wav output.rtvx
```

Decode:

```bash
cargo run --release -- dec output.rtvx reconstructed.wav
```

## WAV requirements

Current prototype input requirements:

- PCM WAV
- mono
- 16-bit little-endian
- 16 kHz

## Validation done in this package

Validated here:

- Python encoder -> Python decoder
- C encoder -> C decoder
- Python encoder output matches C encoder output byte-for-byte on the included sample
- Python decoder output matches C decoder output byte-for-byte on the included sample

Rust source is included and follows the same bitstream specification, but it was not compiled inside this environment because the Rust toolchain was not available here.

## License

MIT License.
