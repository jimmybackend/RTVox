use crate::common::{analyze_frame, pad_samples, read_wav_mono16, write_rtvx, FRAME_SAMPLES};

pub fn run(input_wav: &str, output_rtvx: &str) -> Result<(), String> {
    let samples = pad_samples(read_wav_mono16(input_wav)?);

    let frames = samples
        .chunks(FRAME_SAMPLES)
        .map(analyze_frame)
        .collect::<Vec<_>>();

    write_rtvx(output_rtvx, &frames)?;
    println!(
        "Encoded {} samples into {} RTVX frames -> {}",
        samples.len(),
        frames.len(),
        output_rtvx
    );
    Ok(())
}
