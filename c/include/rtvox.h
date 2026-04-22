#ifndef RTVOX_H
#define RTVOX_H

#include <stdint.h>

#define RTVOX_VERSION 1
#define RTVOX_SAMPLE_RATE 16000
#define RTVOX_CHANNELS 1
#define RTVOX_FRAME_SAMPLES 320
#define RTVOX_BYTES_PER_FRAME 4
#define RTVOX_FLAG_VOICED 0x01

typedef struct {
    uint8_t level_q;
    uint8_t zcr_q;
    uint8_t pitch_q;
    uint8_t flags;
} RtvoxFrame;

typedef struct {
    double phase;
    uint32_t noise_state;
} RtvoxSynthState;

int rtvox_encode_wav_to_file(const char *input_wav, const char *output_rtvx);
int rtvox_decode_file_to_wav(const char *input_rtvx, const char *output_wav);

double rtvox_clamp(double value, double minimum, double maximum);
double rtvox_frame_rms(const int16_t *frame, int frame_samples);
double rtvox_frame_zcr(const int16_t *frame, int frame_samples);
double rtvox_estimate_pitch_hz(const int16_t *frame, int frame_samples, int sample_rate, double *best_corr);
uint8_t rtvox_quantize_level(double rms);
double rtvox_dequantize_level(uint8_t level_q);
uint8_t rtvox_quantize_zcr(double zcr);
double rtvox_dequantize_zcr(uint8_t zcr_q);
uint8_t rtvox_quantize_pitch(double pitch_hz);
double rtvox_dequantize_pitch(uint8_t pitch_q);
RtvoxFrame rtvox_analyze_frame(const int16_t *frame, int frame_samples);
void rtvox_synth_init(RtvoxSynthState *state);
void rtvox_synthesize_frame(const RtvoxFrame *frame, RtvoxSynthState *state, int16_t *out_samples);

#endif
