use std::fs::File;
use std::io::{Read, Write};

pub const MAGIC: &[u8; 4] = b"RTVX";
pub const VERSION: u8 = 1;
pub const SAMPLE_RATE: u32 = 16_000;
pub const CHANNELS: u8 = 1;
pub const FRAME_SAMPLES: usize = 320;
pub const BYTES_PER_FRAME: u8 = 4;
pub const PITCH_MIN_HZ: f64 = 50.0;
pub const PITCH_MAX_HZ: f64 = 400.0;
pub const FLAG_VOICED: u8 = 0x01;

#[derive(Clone, Copy, Debug)]
pub struct RtvoxFrame {
    pub level_q: u8,
    pub zcr_q: u8,
    pub pitch_q: u8,
    pub flags: u8,
}

#[derive(Debug)]
pub struct SynthState {
    pub phase: f64,
    pub noise_state: u32,
}

impl SynthState {
    pub fn new() -> Self {
        Self {
            phase: 0.0,
            noise_state: 0x1234_5678,
        }
    }

    pub fn next_noise(&mut self) -> f64 {
        self.noise_state = self
            .noise_state
            .wrapping_mul(1_664_525)
            .wrapping_add(1_013_904_223);
        (((self.noise_state >> 16) & 0x7FFF) as f64 / 16384.0) - 1.0
    }
}

pub fn clamp(value: f64, minimum: f64, maximum: f64) -> f64 {
    value.max(minimum).min(maximum)
}

pub fn read_wav_mono16(path: &str) -> Result<Vec<i16>, String> {
    let mut file = File::open(path).map_err(|e| format!("Failed to open WAV: {e}"))?;
    let mut data = Vec::new();
    file.read_to_end(&mut data)
        .map_err(|e| format!("Failed to read WAV: {e}"))?;

    if data.len() < 44 {
        return Err("WAV too small".to_string());
    }
    if &data[0..4] != b"RIFF" || &data[8..12] != b"WAVE" {
        return Err("Invalid WAV header".to_string());
    }

    let mut offset = 12usize;
    let mut audio_format = 0u16;
    let mut num_channels = 0u16;
    let mut sample_rate = 0u32;
    let mut bits_per_sample = 0u16;
    let mut sample_bytes: Vec<u8> = Vec::new();

    while offset + 8 <= data.len() {
        let chunk_id = &data[offset..offset + 4];
        let chunk_size =
            u32::from_le_bytes(data[offset + 4..offset + 8].try_into().unwrap()) as usize;
        offset += 8;

        if offset + chunk_size > data.len() {
            return Err("Corrupt WAV chunk".to_string());
        }

        if chunk_id == b"fmt " {
            if chunk_size < 16 {
                return Err("Invalid fmt chunk".to_string());
            }
            audio_format = u16::from_le_bytes(data[offset..offset + 2].try_into().unwrap());
            num_channels = u16::from_le_bytes(data[offset + 2..offset + 4].try_into().unwrap());
            sample_rate = u32::from_le_bytes(data[offset + 4..offset + 8].try_into().unwrap());
            bits_per_sample =
                u16::from_le_bytes(data[offset + 14..offset + 16].try_into().unwrap());
        } else if chunk_id == b"data" {
            sample_bytes = data[offset..offset + chunk_size].to_vec();
        }

        offset += chunk_size;
        if chunk_size & 1 == 1 {
            offset += 1;
        }
    }

    if audio_format != 1 || num_channels != 1 || bits_per_sample != 16 || sample_rate != SAMPLE_RATE {
        return Err("WAV must be PCM mono 16-bit 16000 Hz".to_string());
    }
    if sample_bytes.is_empty() {
        return Err("Missing WAV data chunk".to_string());
    }

    let mut samples = Vec::with_capacity(sample_bytes.len() / 2);
    for chunk in sample_bytes.chunks_exact(2) {
        samples.push(i16::from_le_bytes([chunk[0], chunk[1]]));
    }
    Ok(samples)
}

pub fn write_wav_mono16(path: &str, samples: &[i16]) -> Result<(), String> {
    let mut file = File::create(path).map_err(|e| format!("Failed to create WAV: {e}"))?;
    let data_size = (samples.len() * 2) as u32;
    let riff_size = 36u32 + data_size;
    let byte_rate = SAMPLE_RATE * 2;
    let block_align = 2u16;
    let bits_per_sample = 16u16;

    file.write_all(b"RIFF").map_err(|e| e.to_string())?;
    file.write_all(&riff_size.to_le_bytes())
        .map_err(|e| e.to_string())?;
    file.write_all(b"WAVE").map_err(|e| e.to_string())?;

    file.write_all(b"fmt ").map_err(|e| e.to_string())?;
    file.write_all(&16u32.to_le_bytes())
        .map_err(|e| e.to_string())?;
    file.write_all(&1u16.to_le_bytes())
        .map_err(|e| e.to_string())?;
    file.write_all(&1u16.to_le_bytes())
        .map_err(|e| e.to_string())?;
    file.write_all(&SAMPLE_RATE.to_le_bytes())
        .map_err(|e| e.to_string())?;
    file.write_all(&byte_rate.to_le_bytes())
        .map_err(|e| e.to_string())?;
    file.write_all(&block_align.to_le_bytes())
        .map_err(|e| e.to_string())?;
    file.write_all(&bits_per_sample.to_le_bytes())
        .map_err(|e| e.to_string())?;

    file.write_all(b"data").map_err(|e| e.to_string())?;
    file.write_all(&data_size.to_le_bytes())
        .map_err(|e| e.to_string())?;

    for &s in samples {
        file.write_all(&s.to_le_bytes())
            .map_err(|e| e.to_string())?;
    }

    Ok(())
}

pub fn pad_samples(mut samples: Vec<i16>) -> Vec<i16> {
    let rem = samples.len() % FRAME_SAMPLES;
    if rem != 0 {
        samples.resize(samples.len() + (FRAME_SAMPLES - rem), 0);
    }
    samples
}

pub fn frame_rms(frame: &[i16]) -> f64 {
    if frame.is_empty() {
        return 0.0;
    }
    let energy = frame
        .iter()
        .map(|&s| {
            let x = s as f64 / 32768.0;
            x * x
        })
        .sum::<f64>()
        / frame.len() as f64;
    energy.max(0.0).sqrt()
}

pub fn frame_zcr(frame: &[i16]) -> f64 {
    if frame.len() < 2 {
        return 0.0;
    }
    let mut crossings = 0usize;
    for w in frame.windows(2) {
        let a = w[0];
        let b = w[1];
        if (a >= 0 && b < 0) || (a < 0 && b >= 0) {
            crossings += 1;
        }
    }
    crossings as f64 / (frame.len() - 1) as f64
}

pub fn estimate_pitch_hz(frame: &[i16], sample_rate: u32) -> (f64, f64) {
    let min_lag = (sample_rate as f64 / PITCH_MAX_HZ).floor().max(2.0) as usize;
    let max_lag = ((sample_rate as f64 / PITCH_MIN_HZ).floor() as usize).min(frame.len().saturating_sub(1));
    let mut best_lag = 0usize;
    let mut best_corr = 0.0f64;

    for lag in min_lag..=max_lag {
        let n = frame.len().saturating_sub(lag);
        if n <= 8 {
            continue;
        }
        let mut num = 0.0;
        let mut den_a = 0.0;
        let mut den_b = 0.0;
        for i in 0..n {
            let a = frame[i] as f64;
            let b = frame[i + lag] as f64;
            num += a * b;
            den_a += a * a;
            den_b += b * b;
        }
        if den_a <= 1e-9 || den_b <= 1e-9 {
            continue;
        }
        let corr = num / (den_a * den_b).sqrt();
        if corr > best_corr {
            best_corr = corr;
            best_lag = lag;
        }
    }

    if best_lag == 0 {
        (0.0, 0.0)
    } else {
        (sample_rate as f64 / best_lag as f64, best_corr)
    }
}

pub fn quantize_level(rms: f64) -> u8 {
    (clamp(clamp(rms, 0.0, 1.0).sqrt(), 0.0, 1.0) * 255.0).round() as u8
}

pub fn dequantize_level(level_q: u8) -> f64 {
    let x = level_q as f64 / 255.0;
    x * x
}

pub fn quantize_zcr(zcr: f64) -> u8 {
    (clamp(zcr, 0.0, 1.0) * 255.0).round() as u8
}

pub fn dequantize_zcr(zcr_q: u8) -> f64 {
    zcr_q as f64 / 255.0
}

pub fn quantize_pitch(pitch_hz: f64) -> u8 {
    let p = clamp(pitch_hz, PITCH_MIN_HZ, PITCH_MAX_HZ);
    (((p - PITCH_MIN_HZ) / (PITCH_MAX_HZ - PITCH_MIN_HZ)) * 255.0).round() as u8
}

pub fn dequantize_pitch(pitch_q: u8) -> f64 {
    PITCH_MIN_HZ + (pitch_q as f64 / 255.0) * (PITCH_MAX_HZ - PITCH_MIN_HZ)
}

pub fn analyze_frame(frame: &[i16]) -> RtvoxFrame {
    let rms = frame_rms(frame);
    let zcr = frame_zcr(frame);
    let (pitch_hz, corr) = estimate_pitch_hz(frame, SAMPLE_RATE);
    let voiced = corr > 0.30 && zcr < 0.25 && rms > 0.01;
    RtvoxFrame {
        level_q: quantize_level(rms),
        zcr_q: quantize_zcr(zcr),
        pitch_q: if voiced { quantize_pitch(pitch_hz) } else { 0 },
        flags: if voiced { FLAG_VOICED } else { 0 },
    }
}

pub fn write_rtvx(path: &str, frames: &[RtvoxFrame]) -> Result<(), String> {
    let mut file = File::create(path).map_err(|e| format!("Failed to create RTVX: {e}"))?;
    file.write_all(MAGIC).map_err(|e| e.to_string())?;
    file.write_all(&[VERSION, 1, CHANNELS, BYTES_PER_FRAME])
        .map_err(|e| e.to_string())?;
    file.write_all(&(FRAME_SAMPLES as u16).to_le_bytes())
        .map_err(|e| e.to_string())?;
    file.write_all(&(frames.len() as u32).to_le_bytes())
        .map_err(|e| e.to_string())?;
    for f in frames {
        file.write_all(&[f.level_q, f.zcr_q, f.pitch_q, f.flags])
            .map_err(|e| e.to_string())?;
    }
    Ok(())
}

pub fn read_rtvx(path: &str) -> Result<Vec<RtvoxFrame>, String> {
    let mut file = File::open(path).map_err(|e| format!("Failed to open RTVX: {e}"))?;
    let mut data = Vec::new();
    file.read_to_end(&mut data)
        .map_err(|e| format!("Failed to read RTVX: {e}"))?;

    if data.len() < 14 {
        return Err("RTVX file too small".to_string());
    }
    if &data[0..4] != MAGIC {
        return Err("Invalid RTVX magic".to_string());
    }
    if data[4] != VERSION || data[5] != 1 || data[6] != CHANNELS || data[7] != BYTES_PER_FRAME {
        return Err("Unsupported RTVX header".to_string());
    }
    let frame_samples = u16::from_le_bytes(data[8..10].try_into().unwrap()) as usize;
    let frame_count = u32::from_le_bytes(data[10..14].try_into().unwrap()) as usize;
    if frame_samples != FRAME_SAMPLES {
        return Err("Unsupported frame size".to_string());
    }
    let payload = &data[14..];
    if payload.len() != frame_count * BYTES_PER_FRAME as usize {
        return Err("RTVX payload size mismatch".to_string());
    }

    let mut frames = Vec::with_capacity(frame_count);
    for chunk in payload.chunks_exact(BYTES_PER_FRAME as usize) {
        frames.push(RtvoxFrame {
            level_q: chunk[0],
            zcr_q: chunk[1],
            pitch_q: chunk[2],
            flags: chunk[3],
        });
    }
    Ok(frames)
}

pub fn synthesize_frame(frame: RtvoxFrame, state: &mut SynthState) -> Vec<i16> {
    let rms_hat = dequantize_level(frame.level_q);
    let zcr_hat = dequantize_zcr(frame.zcr_q);
    let voiced = (frame.flags & FLAG_VOICED) != 0;
    let pitch_hz = if voiced {
        dequantize_pitch(frame.pitch_q)
    } else {
        0.0
    };
    let amp = clamp(rms_hat * 46000.0, 0.0, 30000.0);

    let mut out = Vec::with_capacity(FRAME_SAMPLES);
    for _ in 0..FRAME_SAMPLES {
        let noise = state.next_noise();
        let sample = if voiced && pitch_hz > 0.0 {
            let tone = (2.0 * std::f64::consts::PI * state.phase).sin();
            let breath = noise * (0.05 + 0.10 * zcr_hat);
            state.phase += pitch_hz / SAMPLE_RATE as f64;
            if state.phase >= 1.0 {
                state.phase -= state.phase.floor();
            }
            amp * (0.85 * tone + 0.15 * breath)
        } else {
            amp * noise * (0.4 + 0.6 * zcr_hat)
        };

        let s = clamp(sample.round(), -32768.0, 32767.0) as i16;
        out.push(s);
    }
    out
}
