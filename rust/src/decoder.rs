use crate::common::{read_rtvx, synthesize_frame, write_wav_mono16, SynthState};

pub fn run(input_rtvx: &str, output_wav: &str) -> Result<(), String> {
    let frames = read_rtvx(input_rtvx)?;
    let mut state = SynthState::new();
    let mut samples = Vec::new();

    for frame in frames {
        samples.extend(synthesize_frame(frame, &mut state));
    }

    write_wav_mono16(output_wav, &samples)?;
    println!("Decoded RTVX into {}", output_wav);
    Ok(())
}
