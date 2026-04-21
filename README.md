# RTVox
RTVox is an open-source real-time voice codec focused on intelligibility, low bitrate, packet-loss resilience, and easy integration. It includes interoperable encoder/decoder prototypes in C, Python, and Rust for research, VoIP experiments, and future standardization.

# RTVox

RTVox is an open-source real-time voice codec project focused on intelligibility, low bitrate operation, packet-loss resilience, and cross-language interoperability.

The goal of RTVox is not just to compress voice, but to preserve speech clarity in real-world calls under unstable network conditions, limited bandwidth, and low-latency requirements.

## Project Goals

- Real-time voice transmission
- Low bitrate operation
- High intelligibility in bad network conditions
- Packet-loss resilience
- Simple and portable bitstream design
- Open specification for public use
- Reference implementations in:
  - C
  - Python
  - Rust

## Why RTVox

Most traditional audio codecs are designed to balance quality and compression for general audio. RTVox is designed with a stronger focus on **voice calls**, where the most important factor is not perfect fidelity, but **understanding speech clearly**.

RTVox is being designed to explore:

- speech-first compression
- adaptive bitrate strategies
- packet-loss-aware voice reconstruction
- low-complexity real-time encoding and decoding
- interoperability across multiple programming languages
- future integration with RTP, SIP, WebRTC, and VoIP platforms

## Current Status

RTVox is currently in the **research and prototype stage**.

This repository is intended to host:

- the public codec specification
- the reference bitstream format
- encoder and decoder prototypes
- test vectors
- network-loss simulations
- cross-language interoperability tests

## Planned Features

- Mono voice codec optimized for calls
- 16 kHz voice mode
- 20 ms frame size
- Multiple bitrate profiles
- Packet loss concealment (PLC)
- Voice activity handling
- Optional redundancy for critical speech frames
- Portable C core
- Python prototype for experiments
- Rust implementation for modern systems integration

## Target Use Cases

- VoIP calls
- SIP softphones
- WebRTC experiments
- embedded voice systems
- low-bandwidth communication
- unstable mobile or rural networks
- research and academic audio coding work

## Design Principles

RTVox follows these principles:

1. **Speech intelligibility first**  
   The codec should prioritize understanding spoken words over preserving full audio fidelity.

2. **Low latency**  
   The codec should be usable in live calls and interactive systems.

3. **Resilience over perfection**  
   In poor conditions, the codec should remain understandable rather than fail gracefully into silence or distortion.

4. **Open and interoperable**  
   The format, bitstream, and implementations should be public and usable by anyone.

5. **Portable implementation**  
   The reference implementation should be lightweight and easy to build across systems.

## Repository Structure

Planned structure:

```text
rtvox/
├── docs/
│   ├── spec-v0.md
│   ├── bitstream.md
│   ├── rtp-mapping.md
│   └── test-vectors.md
├── include/
│   └── rtvox.h
├── src/
│   ├── encoder.c
│   ├── decoder.c
│   ├── bitstream.c
│   ├── plc.c
│   └── packet_loss.c
├── python/
│   ├── encoder.py
│   ├── decoder.py
│   └── tools/
├── rust/
│   ├── src/
│   └── Cargo.toml
├── tests/
│   ├── roundtrip/
│   ├── vectors/
│   └── network-loss/
├── examples/
│   ├── cli/
│   ├── rtp/
│   └── voip/
├── LICENSE
├── README.md
└── CMakeLists.txt
