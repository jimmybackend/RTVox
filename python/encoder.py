import sys

from common import FRAME_SAMPLES, analyze_frame, pad_samples, read_wav_mono16, write_rtvx


def main():
    if len(sys.argv) != 3:
        print("Usage: python3 encoder.py input.wav output.rtvx")
        sys.exit(1)

    input_wav = sys.argv[1]
    output_rtvx = sys.argv[2]

    samples = read_wav_mono16(input_wav)
    samples = pad_samples(samples)

    frames = []
    for i in range(0, len(samples), FRAME_SAMPLES):
        frame = samples[i:i + FRAME_SAMPLES]
        frames.append(analyze_frame(frame))

    write_rtvx(output_rtvx, frames)
    print(f"Encoded {len(samples)} samples into {len(frames)} RTVX frames -> {output_rtvx}")


if __name__ == "__main__":
    main()
