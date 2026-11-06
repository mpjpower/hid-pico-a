#ifndef IR_RECEIVER_H
#define IR_RECEIVER_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t samples_captured;
    uint32_t blocks_completed;
    uint32_t fifo_overruns;
    uint16_t last_block_min;
    uint16_t last_block_max;
} ir_receiver_acquisition_status_t;

typedef struct {
    uint32_t bursts_detected;
    uint32_t last_leading_edge_sample;
    uint32_t last_interval_samples;
    uint16_t baseline;
    uint16_t noise;
    uint16_t on_threshold;
    uint16_t off_threshold;
    uint16_t last_burst_peak;
    uint16_t background_score;
    uint16_t background_deviation;
    uint16_t pulse_score;
    uint16_t first_candidate_peak;
} ir_receiver_burst_status_t;

typedef struct {
    uint32_t starts_detected;
    uint32_t frames_completed;
    uint32_t timing_errors;
    uint32_t parity_errors;
    uint32_t valid_frames;
    uint16_t last_code;
    uint8_t last_byte;
    uint32_t last_half_bit_mask;
    uint8_t last_frame_edges;
    uint8_t last_frame_rejected_edges;
    uint8_t last_slot;
} ir_receiver_decode_status_t;

typedef struct {
    uint32_t failed_frames;
    uint32_t half_bit_mask;
    uint16_t code;
    uint8_t edges;
    uint8_t rejected_edges;
} ir_receiver_failure_status_t;

void ir_receiver_init(void);
void ir_receiver_get_acquisition_status(ir_receiver_acquisition_status_t *status);
void ir_receiver_get_burst_status(ir_receiver_burst_status_t *status);
void ir_receiver_get_decode_status(ir_receiver_decode_status_t *status);
void ir_receiver_get_failure_status(ir_receiver_failure_status_t *status);
size_t ir_receiver_get_failed_edge_times(uint16_t *times, size_t max_count);
size_t ir_receiver_read(uint8_t *buffer, size_t max_count);

#endif // IR_RECEIVER_H