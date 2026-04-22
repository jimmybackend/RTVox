import sys

from common import SynthState, read_rtvx, synthesize_frame, write_wav_mono16


def main():
    if len(sys.argv) != 3:
        print("Usage: python3 decoder.py input.rtvx output.wav")
        sys.exit(1)

    input_rtvx = sys.argv[1]
    output_wav = sys.argv[2]

    frames = read_rtvx(input_rtvx)
    state = SynthState()
    samples = []

    for frame in frames:
        samples.extend(synthesize_frame(frame, state))

    write_wav_mono16(output_wav, samples)
    print(f"Decoded {len(frames)} RTVX frames -> {output_wav}")


if __name__ == "__main__":
    main()
