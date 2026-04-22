#include "rtvox.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RTVOX_PITCH_MIN_HZ 50.0
#define RTVOX_PITCH_MAX_HZ 400.0

typedef struct {
    int16_t *samples;
    uint32_t sample_count;
    uint32_t sample_rate;
} WavData;

static int read_le16(FILE *f, uint16_t *value) {
    unsigned char b[2];
    if (fread(b, 1, 2, f) != 2) return 0;
    *value = (uint16_t)(b[0] | (b[1] << 8));
    return 1;
}

static int read_le32(FILE *f, uint32_t *value) {
    unsigned char b[4];
    if (fread(b, 1, 4, f) != 4) return 0;
    *value = (uint32_t)(b[0] | (b[1] << 8) | (b[2] << 16) | ((uint32_t)b[3] << 24));
    return 1;
}

static int write_le16(FILE *f, uint16_t value) {
    unsigned char b[2];
    b[0] = (unsigned char)(value & 0xFF);
    b[1] = (unsigned char)((value >> 8) & 0xFF);
    return fwrite(b, 1, 2, f) == 2;
}

static int write_le32(FILE *f, uint32_t value) {
    unsigned char b[4];
    b[0] = (unsigned char)(value & 0xFF);
    b[1] = (unsigned char)((value >> 8) & 0xFF);
    b[2] = (unsigned char)((value >> 16) & 0xFF);
    b[3] = (unsigned char)((value >> 24) & 0xFF);
    return fwrite(b, 1, 4, f) == 4;
}

static int read_pcm16_mono_wav(const char *path, WavData *out) {
    FILE *f = fopen(path, "rb");
    char id[4];
    uint32_t chunk_size = 0;
    uint16_t audio_format = 0;
    uint16_t num_channels = 0;
    uint32_t sample_rate = 0;
    uint16_t bits_per_sample = 0;
    int16_t *samples = NULL;
    uint32_t sample_count = 0;
    int fmt_found = 0;
    int data_found = 0;

    if (!f) {
        fprintf(stderr, "Failed to open WAV: %s\n", path);
        return 0;
    }

    if (fread(id, 1, 4, f) != 4 || memcmp(id, "RIFF", 4) != 0) {
        fprintf(stderr, "Invalid WAV: missing RIFF\n");
        fclose(f);
        return 0;
    }
    if (!read_le32(f, &chunk_size)) {
        fclose(f);
        return 0;
    }
    (void)chunk_size;
    if (fread(id, 1, 4, f) != 4 || memcmp(id, "WAVE", 4) != 0) {
        fprintf(stderr, "Invalid WAV: missing WAVE\n");
        fclose(f);
        return 0;
    }

    while (fread(id, 1, 4, f) == 4) {
        if (!read_le32(f, &chunk_size)) {
            fclose(f);
            return 0;
        }

        if (memcmp(id, "fmt ", 4) == 0) {
            uint16_t block_align = 0;
            uint32_t byte_rate = 0;
            if (!read_le16(f, &audio_format) ||
                !read_le16(f, &num_channels) ||
                !read_le32(f, &sample_rate) ||
                !read_le32(f, &byte_rate) ||
                !read_le16(f, &block_align) ||
                !read_le16(f, &bits_per_sample)) {
                fclose(f);
                return 0;
            }
            if (chunk_size > 16) {
                if (fseek(f, (long)(chunk_size - 16), SEEK_CUR) != 0) {
                    fclose(f);
                    return 0;
                }
            }
            fmt_found = 1;
            (void)byte_rate;
            (void)block_align;
        } else if (memcmp(id, "data", 4) == 0) {
            samples = (int16_t *)malloc(chunk_size);
            if (!samples) {
                fprintf(stderr, "Out of memory reading WAV data\n");
                fclose(f);
                return 0;
            }
            if (fread(samples, 1, chunk_size, f) != chunk_size) {
                fprintf(stderr, "Failed reading WAV sample data\n");
                free(samples);
                fclose(f);
                return 0;
            }
            sample_count = chunk_size / 2;
            data_found = 1;
        } else {
            if (fseek(f, (long)chunk_size, SEEK_CUR) != 0) {
                fclose(f);
                free(samples);
                return 0;
            }
        }

        if (chunk_size & 1U) {
            if (fseek(f, 1, SEEK_CUR) != 0) {
                fclose(f);
                free(samples);
                return 0;
            }
        }
    }

    fclose(f);

    if (!fmt_found || !data_found) {
        fprintf(stderr, "Invalid WAV: missing fmt or data\n");
        free(samples);
        return 0;
    }
    if (audio_format != 1 || num_channels != 1 || bits_per_sample != 16 || sample_rate != RTVOX_SAMPLE_RATE) {
        fprintf(stderr, "WAV must be PCM mono 16-bit 16000 Hz\n");
        free(samples);
        return 0;
    }

    out->samples = samples;
    out->sample_count = sample_count;
    out->sample_rate = sample_rate;
    return 1;
}

static int write_pcm16_mono_wav(const char *path, const int16_t *samples, uint32_t sample_count, uint32_t sample_rate) {
    FILE *f = fopen(path, "wb");
    uint32_t data_size = sample_count * 2U;
    uint32_t riff_size = 36U + data_size;
    uint32_t byte_rate = sample_rate * 2U;
    uint16_t block_align = 2;
    uint16_t bits_per_sample = 16;

    if (!f) {
        fprintf(stderr, "Failed to open output WAV: %s\n", path);
        return 0;
    }

    if (fwrite("RIFF", 1, 4, f) != 4 ||
        !write_le32(f, riff_size) ||
        fwrite("WAVE", 1, 4, f) != 4 ||
        fwrite("fmt ", 1, 4, f) != 4 ||
        !write_le32(f, 16) ||
        !write_le16(f, 1) ||
        !write_le16(f, 1) ||
        !write_le32(f, sample_rate) ||
        !write_le32(f, byte_rate) ||
        !write_le16(f, block_align) ||
        !write_le16(f, bits_per_sample) ||
        fwrite("data", 1, 4, f) != 4 ||
        !write_le32(f, data_size)) {
        fclose(f);
        return 0;
    }

    if (fwrite(samples, sizeof(int16_t), sample_count, f) != sample_count) {
        fclose(f);
        return 0;
    }

    fclose(f);
    return 1;
}

double rtvox_clamp(double value, double minimum, double maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

double rtvox_frame_rms(const int16_t *frame, int frame_samples) {
    int i;
    double energy = 0.0;
    if (frame_samples <= 0) return 0.0;
    for (i = 0; i < frame_samples; ++i) {
        double x = frame[i] / 32768.0;
        energy += x * x;
    }
    energy /= (double)frame_samples;
    return sqrt(energy > 0.0 ? energy : 0.0);
}

double rtvox_frame_zcr(const int16_t *frame, int frame_samples) {
    int i;
    int crossings = 0;
    if (frame_samples < 2) return 0.0;
    for (i = 0; i < frame_samples - 1; ++i) {
        if ((frame[i] >= 0 && frame[i + 1] < 0) || (frame[i] < 0 && frame[i + 1] >= 0)) {
            crossings++;
        }
    }
    return crossings / (double)(frame_samples - 1);
}

double rtvox_estimate_pitch_hz(const int16_t *frame, int frame_samples, int sample_rate, double *best_corr) {
    int min_lag = (int)(sample_rate / RTVOX_PITCH_MAX_HZ);
    int max_lag = (int)(sample_rate / RTVOX_PITCH_MIN_HZ);
    int lag;
    int best_lag = 0;
    double corr_best = 0.0;

    if (min_lag < 2) min_lag = 2;
    if (max_lag >= frame_samples) max_lag = frame_samples - 1;

    for (lag = min_lag; lag <= max_lag; ++lag) {
        int i;
        int n = frame_samples - lag;
        double num = 0.0;
        double den_a = 0.0;
        double den_b = 0.0;
        double corr;

        if (n <= 8) continue;

        for (i = 0; i < n; ++i) {
            double a = (double)frame[i];
            double b = (double)frame[i + lag];
            num += a * b;
            den_a += a * a;
            den_b += b * b;
        }

        if (den_a <= 1e-9 || den_b <= 1e-9) continue;
        corr = num / sqrt(den_a * den_b);
        if (corr > corr_best) {
            corr_best = corr;
            best_lag = lag;
        }
    }

    if (best_corr) *best_corr = corr_best;
    if (best_lag <= 0) return 0.0;
    return sample_rate / (double)best_lag;
}

uint8_t rtvox_quantize_level(double rms) {
    double x = sqrt(rtvox_clamp(rms, 0.0, 1.0));
    return (uint8_t)lrint(rtvox_clamp(x, 0.0, 1.0) * 255.0);
}

double rtvox_dequantize_level(uint8_t level_q) {
    double x = level_q / 255.0;
    return x * x;
}

uint8_t rtvox_quantize_zcr(double zcr) {
    return (uint8_t)lrint(rtvox_clamp(zcr, 0.0, 1.0) * 255.0);
}

double rtvox_dequantize_zcr(uint8_t zcr_q) {
    return zcr_q / 255.0;
}

uint8_t rtvox_quantize_pitch(double pitch_hz) {
    double norm = (rtvox_clamp(pitch_hz, RTVOX_PITCH_MIN_HZ, RTVOX_PITCH_MAX_HZ) - RTVOX_PITCH_MIN_HZ) /
                  (RTVOX_PITCH_MAX_HZ - RTVOX_PITCH_MIN_HZ);
    return (uint8_t)lrint(norm * 255.0);
}

double rtvox_dequantize_pitch(uint8_t pitch_q) {
    return RTVOX_PITCH_MIN_HZ + (pitch_q / 255.0) * (RTVOX_PITCH_MAX_HZ - RTVOX_PITCH_MIN_HZ);
}

RtvoxFrame rtvox_analyze_frame(const int16_t *frame, int frame_samples) {
    RtvoxFrame out;
    double rms = rtvox_frame_rms(frame, frame_samples);
    double zcr = rtvox_frame_zcr(frame, frame_samples);
    double corr = 0.0;
    double pitch_hz = rtvox_estimate_pitch_hz(frame, frame_samples, RTVOX_SAMPLE_RATE, &corr);
    int voiced = (corr > 0.30 && zcr < 0.25 && rms > 0.01);

    out.level_q = rtvox_quantize_level(rms);
    out.zcr_q = rtvox_quantize_zcr(zcr);
    out.pitch_q = voiced ? rtvox_quantize_pitch(pitch_hz) : 0;
    out.flags = voiced ? RTVOX_FLAG_VOICED : 0;
    return out;
}

void rtvox_synth_init(RtvoxSynthState *state) {
    state->phase = 0.0;
    state->noise_state = 0x12345678U;
}

static double rtvox_next_noise(RtvoxSynthState *state) {
    state->noise_state = (uint32_t)(1664525U * state->noise_state + 1013904223U);
    return (((state->noise_state >> 16) & 0x7FFFU) / 16384.0) - 1.0;
}

void rtvox_synthesize_frame(const RtvoxFrame *frame, RtvoxSynthState *state, int16_t *out_samples) {
    int i;
    double rms_hat = rtvox_dequantize_level(frame->level_q);
    double zcr_hat = rtvox_dequantize_zcr(frame->zcr_q);
    int voiced = (frame->flags & RTVOX_FLAG_VOICED) != 0;
    double pitch_hz = voiced ? rtvox_dequantize_pitch(frame->pitch_q) : 0.0;
    double amp = rtvox_clamp(rms_hat * 46000.0, 0.0, 30000.0);

    for (i = 0; i < RTVOX_FRAME_SAMPLES; ++i) {
        double noise = rtvox_next_noise(state);
        double sample;

        if (voiced && pitch_hz > 0.0) {
            double tone = sin(2.0 * 3.14159265358979323846 * state->phase);
            double breath = noise * (0.05 + 0.10 * zcr_hat);
            sample = amp * (0.85 * tone + 0.15 * breath);
            state->phase += pitch_hz / RTVOX_SAMPLE_RATE;
            if (state->phase >= 1.0) {
                state->phase -= floor(state->phase);
            }
        } else {
            sample = amp * noise * (0.4 + 0.6 * zcr_hat);
        }

        if (sample > 32767.0) sample = 32767.0;
        if (sample < -32768.0) sample = -32768.0;
        out_samples[i] = (int16_t)lrint(sample);
    }
}

static int write_rtvx_file(const char *path, const RtvoxFrame *frames, uint32_t frame_count) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "Failed to open output RTVX: %s\n", path);
        return 0;
    }

    if (fwrite("RTVX", 1, 4, f) != 4 ||
        fputc(RTVOX_VERSION, f) == EOF ||
        fputc(1, f) == EOF ||
        fputc(RTVOX_CHANNELS, f) == EOF ||
        fputc(RTVOX_BYTES_PER_FRAME, f) == EOF ||
        !write_le16(f, RTVOX_FRAME_SAMPLES) ||
        !write_le32(f, frame_count)) {
        fclose(f);
        return 0;
    }

    if (fwrite(frames, sizeof(RtvoxFrame), frame_count, f) != frame_count) {
        fclose(f);
        return 0;
    }

    fclose(f);
    return 1;
}

static int read_rtvx_file(const char *path, RtvoxFrame **frames_out, uint32_t *frame_count_out) {
    FILE *f = fopen(path, "rb");
    char magic[4];
    uint16_t frame_samples = 0;
    uint32_t frame_count = 0;
    int version, sample_rate_code, channels, bytes_per_frame;
    RtvoxFrame *frames = NULL;

    if (!f) {
        fprintf(stderr, "Failed to open RTVX: %s\n", path);
        return 0;
    }

    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "RTVX", 4) != 0) {
        fprintf(stderr, "Invalid RTVX magic\n");
        fclose(f);
        return 0;
    }

    version = fgetc(f);
    sample_rate_code = fgetc(f);
    channels = fgetc(f);
    bytes_per_frame = fgetc(f);

    if (version != RTVOX_VERSION || sample_rate_code != 1 || channels != RTVOX_CHANNELS || bytes_per_frame != RTVOX_BYTES_PER_FRAME ||
        !read_le16(f, &frame_samples) || !read_le32(f, &frame_count)) {
        fprintf(stderr, "Unsupported RTVX header\n");
        fclose(f);
        return 0;
    }

    if (frame_samples != RTVOX_FRAME_SAMPLES) {
        fprintf(stderr, "Unsupported frame size in RTVX file\n");
        fclose(f);
        return 0;
    }

    frames = (RtvoxFrame *)malloc(sizeof(RtvoxFrame) * frame_count);
    if (!frames) {
        fprintf(stderr, "Out of memory reading RTVX frames\n");
        fclose(f);
        return 0;
    }

    if (fread(frames, sizeof(RtvoxFrame), frame_count, f) != frame_count) {
        fprintf(stderr, "RTVX payload read error\n");
        free(frames);
        fclose(f);
        return 0;
    }

    fclose(f);
    *frames_out = frames;
    *frame_count_out = frame_count;
    return 1;
}

int rtvox_encode_wav_to_file(const char *input_wav, const char *output_rtvx) {
    WavData wav;
    uint32_t padded_count;
    uint32_t frame_count;
    int16_t *padded_samples = NULL;
    RtvoxFrame *frames = NULL;
    uint32_t i;
    int ok = 0;

    if (!read_pcm16_mono_wav(input_wav, &wav)) {
        return 0;
    }

    padded_count = wav.sample_count;
    if (padded_count % RTVOX_FRAME_SAMPLES != 0) {
        padded_count += RTVOX_FRAME_SAMPLES - (padded_count % RTVOX_FRAME_SAMPLES);
    }

    padded_samples = (int16_t *)calloc(padded_count, sizeof(int16_t));
    if (!padded_samples) {
        fprintf(stderr, "Out of memory padding samples\n");
        free(wav.samples);
        return 0;
    }
    memcpy(padded_samples, wav.samples, wav.sample_count * sizeof(int16_t));
    free(wav.samples);

    frame_count = padded_count / RTVOX_FRAME_SAMPLES;
    frames = (RtvoxFrame *)malloc(sizeof(RtvoxFrame) * frame_count);
    if (!frames) {
        fprintf(stderr, "Out of memory allocating frames\n");
        free(padded_samples);
        return 0;
    }

    for (i = 0; i < frame_count; ++i) {
        frames[i] = rtvox_analyze_frame(&padded_samples[i * RTVOX_FRAME_SAMPLES], RTVOX_FRAME_SAMPLES);
    }

    ok = write_rtvx_file(output_rtvx, frames, frame_count);

    free(frames);
    free(padded_samples);
    return ok;
}

int rtvox_decode_file_to_wav(const char *input_rtvx, const char *output_wav) {
    RtvoxFrame *frames = NULL;
    uint32_t frame_count = 0;
    int16_t *samples = NULL;
    uint32_t total_samples;
    uint32_t i;
    RtvoxSynthState state;
    int ok = 0;

    if (!read_rtvx_file(input_rtvx, &frames, &frame_count)) {
        return 0;
    }

    total_samples = frame_count * RTVOX_FRAME_SAMPLES;
    samples = (int16_t *)malloc(sizeof(int16_t) * total_samples);
    if (!samples) {
        fprintf(stderr, "Out of memory allocating decoded samples\n");
        free(frames);
        return 0;
    }

    rtvox_synth_init(&state);
    for (i = 0; i < frame_count; ++i) {
        rtvox_synthesize_frame(&frames[i], &state, &samples[i * RTVOX_FRAME_SAMPLES]);
    }

    ok = write_pcm16_mono_wav(output_wav, samples, total_samples, RTVOX_SAMPLE_RATE);

    free(samples);
    free(frames);
    return ok;
}
