#include "ir_receiver.h"

#include "hardware/adc.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/regs/adc.h"
#include "hardware/regs/dreq.h"
#include "hardware/structs/adc.h"
#include "hardware/structs/dma.h"
#include "hardware/sync.h"

#include <string.h>

#define IR_RECEIVER_ADC_GPIO 26u
#define IR_RECEIVER_ADC_INPUT 0u
#define IR_DMA_BLOCK_SAMPLES 256u
#define IR_DMA_BLOCK_RING_BITS 9u
#define IR_BASELINE_SHIFT 5u
#define IR_NOISE_SHIFT 5u
#define IR_CARRIER_LAG_SAMPLES 15u
#define IR_CARRIER_WINDOW_SAMPLES 32u
#define IR_CARRIER_SCORE_SCALE 1024u
#define IR_BACKGROUND_AVERAGE_SHIFT 5u
#define IR_BACKGROUND_DEVIATION_SHIFT 5u
#define IR_PULSE_AVERAGE_SHIFT 3u
#define IR_BACKGROUND_WARMUP_SAMPLES 256u
#define IR_MIN_ON_MARGIN 24u
#define IR_MIN_OFF_MARGIN 12u
#define IR_ON_DEVIATION_MULTIPLIER 6u
#define IR_OFF_DEVIATION_MULTIPLIER 3u
#define IR_UNLEARNED_ON_DEVIATION_MULTIPLIER 4u
#define IR_UNLEARNED_OFF_DEVIATION_MULTIPLIER 2u
#define IR_UNLEARNED_ON_MARGIN 450u
#define IR_UNLEARNED_OFF_MARGIN 225u
#define IR_MIN_PULSE_SCORE 400u
#define IR_MIN_SIGNAL_ENERGY 128u
#define IR_HALF_BIT_SAMPLES 214u
#define IR_MIN_EDGE_SPACING_SAMPLES 150u
#define IR_START_TOLERANCE_SAMPLES 45u
#define IR_SLOT_TOLERANCE_SAMPLES 70u
#define IR_DATA_HALF_BITS 24u
#define IR_CODE_BITS 12u
#define IR_RX_RING_SIZE 256u
#define IR_FAILED_FRAME_MIN_EDGES 6u
#define IR_FAILED_EDGE_TIMES 12u
#define IR_FRAME_MAX_EDGES 32u
#define IR_HALF_BIT_X16 3418
#define IR_RX_RING_MASK (IR_RX_RING_SIZE - 1u)

_Alignas(1u << IR_DMA_BLOCK_RING_BITS)
static uint16_t s_dma_samples[2][IR_DMA_BLOCK_SAMPLES];
static uint s_dma_channels[2];
static uint32_t s_dma_channel_mask;

static volatile uint32_t s_samples_captured;
static volatile uint32_t s_blocks_completed;
static volatile uint32_t s_fifo_overruns;
static volatile uint16_t s_last_block_min;
static volatile uint16_t s_last_block_max;
static uint32_t s_sample_index;
static volatile int32_t s_baseline_q8;
static volatile int32_t s_noise_q8;
static int32_t s_background_score_q8;
static int32_t s_background_deviation_q8;
static int32_t s_pulse_score_q8;
static uint32_t s_background_samples;
static bool s_background_initialized;
static bool s_pulse_score_initialized;
static bool s_baseline_initialized;
static int16_t s_carrier_history[IR_CARRIER_LAG_SAMPLES];
static int32_t s_correlation_window[IR_CARRIER_WINDOW_SAMPLES];
static int32_t s_correlation_sum;
static uint32_t s_energy_window[IR_CARRIER_WINDOW_SAMPLES];
static uint32_t s_energy_sum;
static uint s_carrier_history_index;
static uint s_correlation_window_index;
static uint s_correlation_samples;
static volatile uint16_t s_on_threshold;
static volatile uint16_t s_off_threshold;
static uint16_t s_burst_peak;
static volatile uint32_t s_bursts_detected;
static volatile uint32_t s_last_leading_edge_sample;
static volatile uint32_t s_last_interval_samples;
static volatile uint16_t s_last_burst_peak;
static volatile uint16_t s_first_candidate_peak;

static void ir_receiver_update_score_thresholds(void) {
    uint32_t background = (uint32_t) (s_background_score_q8 >> 8);
    uint32_t deviation = (uint32_t) (s_background_deviation_q8 >> 8);
    uint32_t on_margin = deviation * IR_ON_DEVIATION_MULTIPLIER;
    uint32_t off_margin = deviation * IR_OFF_DEVIATION_MULTIPLIER;

    if (on_margin < IR_MIN_ON_MARGIN) {
        on_margin = IR_MIN_ON_MARGIN;
    }
    if (off_margin < IR_MIN_OFF_MARGIN) {
        off_margin = IR_MIN_OFF_MARGIN;
    }

    uint32_t on_threshold;
    uint32_t off_threshold;
    uint32_t pulse = (uint32_t) (s_pulse_score_q8 >> 8);
    if (s_pulse_score_initialized && pulse > background) {
        uint32_t signal_gap = pulse - background;
        on_margin = signal_gap / 2u;
        off_margin = signal_gap / 4u;
    } else {
        on_margin = IR_UNLEARNED_ON_MARGIN +
                    deviation * IR_UNLEARNED_ON_DEVIATION_MULTIPLIER;
        off_margin = IR_UNLEARNED_OFF_MARGIN +
                     deviation * IR_UNLEARNED_OFF_DEVIATION_MULTIPLIER;
    }

    if (on_margin < IR_MIN_ON_MARGIN) {
        on_margin = IR_MIN_ON_MARGIN;
    }
    if (off_margin < IR_MIN_OFF_MARGIN) {
        off_margin = IR_MIN_OFF_MARGIN;
    }
    if (off_margin >= on_margin) {
        off_margin = on_margin > 1u ? on_margin - 1u : 0u;
    }

    on_threshold = background + on_margin;
    off_threshold = background + off_margin;

    if (on_threshold >= IR_CARRIER_SCORE_SCALE) {
        on_threshold = IR_CARRIER_SCORE_SCALE - 1u;
    }
    if (off_threshold >= on_threshold) {
        off_threshold = on_threshold - 1u;
    }

    s_on_threshold = (uint16_t) on_threshold;
    s_off_threshold = (uint16_t) off_threshold;
}

static void ir_receiver_update_background_score(uint16_t score) {
    int32_t score_q8 = (int32_t) score << 8;
    if (!s_background_initialized) {
        s_background_score_q8 = score_q8;
        s_background_deviation_q8 = 0;
        s_background_initialized = true;
    } else {
        int32_t delta = score_q8 - s_background_score_q8;
        int32_t absolute_delta = delta < 0 ? -delta : delta;
        s_background_score_q8 += delta >> IR_BACKGROUND_AVERAGE_SHIFT;
        s_background_deviation_q8 +=
            (absolute_delta - s_background_deviation_q8) >> IR_BACKGROUND_DEVIATION_SHIFT;
    }

    if (s_background_samples < IR_BACKGROUND_WARMUP_SAMPLES) {
        s_background_samples++;
    }
    ir_receiver_update_score_thresholds();
}

static void ir_receiver_update_pulse_score(uint16_t score) {
    if (score < IR_MIN_PULSE_SCORE) {
        return;
    }
    int32_t score_q8 = (int32_t) score << 8;
    if (!s_pulse_score_initialized) {
        s_pulse_score_q8 = score_q8;
        s_pulse_score_initialized = true;
    } else {
        s_pulse_score_q8 += (score_q8 - s_pulse_score_q8) >> IR_PULSE_AVERAGE_SHIFT;
    }
}
static bool s_have_last_edge;
static bool s_in_burst;
static uint32_t s_previous_edges[2];
static uint8_t s_previous_edge_count;
static bool s_frame_collecting;
static bool s_frame_timing_invalid;
static uint32_t s_frame_origin_sample;
static bool s_frame_hits[IR_DATA_HALF_BITS];
static uint8_t s_frame_edge_count;
static uint8_t s_frame_rejected_edges;
static volatile uint32_t s_last_half_bit_mask;
static volatile uint8_t s_last_frame_edges;
static volatile uint8_t s_last_frame_rejected_edges;
static volatile uint8_t s_last_slot;
static volatile uint32_t s_starts_detected;
static volatile uint32_t s_frames_completed;
static volatile uint32_t s_timing_errors;
static volatile uint32_t s_parity_errors;
static volatile uint32_t s_valid_frames;
static volatile uint16_t s_last_code;
static volatile uint8_t s_last_byte;
static volatile uint32_t s_failed_frames;
static volatile uint32_t s_failed_half_bit_mask;
static volatile uint16_t s_failed_code;
static volatile uint8_t s_failed_edges;
static volatile uint8_t s_failed_rejected_edges;
static uint16_t s_frame_edge_times[IR_FRAME_MAX_EDGES];
static uint8_t s_frame_edge_time_count;
static volatile uint16_t s_failed_edge_times[IR_FAILED_EDGE_TIMES];
static volatile uint8_t s_failed_edge_time_count;
static uint8_t s_rx_ring[IR_RX_RING_SIZE];
static volatile uint16_t s_rx_head;
static volatile uint16_t s_rx_tail;

static void ir_receiver_queue_byte(uint8_t byte) {
    uint16_t next_head = (uint16_t) ((s_rx_head + 1u) & IR_RX_RING_MASK);
    if (next_head != s_rx_tail) {
        s_rx_ring[s_rx_head] = byte;
        s_rx_head = next_head;
    }
}

static void ir_receiver_clear_fifo_overrun(void) {
    uint32_t fifo_config = adc_hw->fcs & (ADC_FCS_THRESH_BITS |
                                         ADC_FCS_DREQ_EN_BITS |
                                         ADC_FCS_ERR_BITS |
                                         ADC_FCS_SHIFT_BITS |
                                         ADC_FCS_EN_BITS);
    adc_hw->fcs = fifo_config | ADC_FCS_OVER_BITS;
}

static bool ir_receiver_near_half_bit(uint32_t interval) {
    uint32_t difference = interval > IR_HALF_BIT_SAMPLES ?
                          interval - IR_HALF_BIT_SAMPLES :
                          IR_HALF_BIT_SAMPLES - interval;
    return difference <= IR_START_TOLERANCE_SAMPLES;
}

static bool ir_receiver_check_parity(uint16_t code) {
    uint8_t data = (uint8_t) code;
    uint8_t parity = (uint8_t) ((code >> 8) & 0x0fu);

    return (((parity >> 3) & 1u) ^ ((data >> 6) & 1u) ^
            ((data >> 5) & 1u) ^ ((data >> 4) & 1u) ^
            ((data >> 3) & 1u)) == 0u &&
           (((parity >> 2) & 1u) ^ ((data >> 7) & 1u) ^
            ((data >> 6) & 1u) ^ ((data >> 5) & 1u) ^
            ((data >> 2) & 1u) ^ ((data >> 1) & 1u)) == 0u &&
           (((parity >> 1) & 1u) ^ ((data >> 7) & 1u) ^
            ((data >> 6) & 1u) ^ ((data >> 4) & 1u) ^
            ((data >> 2) & 1u) ^ (data & 1u)) == 0u &&
           ((parity & 1u) ^ ((data >> 7) & 1u) ^
            ((data >> 3) & 1u) ^ ((data >> 1) & 1u) ^
            (data & 1u)) == 0u;
}

static int32_t ir_receiver_round_slots(int32_t delta_x16) {
    int32_t half_bit = IR_HALF_BIT_X16;
    return (delta_x16 + (delta_x16 < 0 ? -half_bit : half_bit) / 2) / half_bit;
}

static bool ir_receiver_build_code(uint16_t *code_out) {
    uint16_t code = 0;
    bool pairs_valid = true;

    for (uint bit_index = 0; bit_index < IR_CODE_BITS; bit_index++) {
        bool first_half_hit = s_frame_hits[bit_index * 2u];
        bool second_half_hit = s_frame_hits[bit_index * 2u + 1u];
        if (first_half_hit == second_half_hit) {
            pairs_valid = false;
        }
        code = (uint16_t) ((code << 1) | (first_half_hit ? 1u : 0u));
    }

    *code_out = code;
    return pairs_valid;
}

static void ir_receiver_fill_slots(int32_t reference, int32_t reference_slot, bool overflow) {
    int32_t count = (int32_t) s_frame_edge_time_count;
    int32_t half_bit = IR_HALF_BIT_X16;
    int32_t tolerance = (int32_t) IR_SLOT_TOLERANCE_SAMPLES * 16;

    memset(s_frame_hits, 0, sizeof(s_frame_hits));
    s_frame_edge_count = 0;
    s_frame_rejected_edges = 0;
    s_frame_timing_invalid = overflow;

    for (int32_t i = 0; i < count; i++) {
        int32_t delta = ((int32_t) s_frame_edge_times[i] -
                         (int32_t) s_frame_edge_times[reference]) * 16;
        int32_t slots = ir_receiver_round_slots(delta);
        int32_t residual = delta - slots * half_bit;
        int32_t slot = reference_slot + slots;

        s_last_slot = (uint8_t) (slot < 0 ? 0 : (slot > UINT8_MAX ? UINT8_MAX : slot));
        if ((residual < 0 ? -residual : residual) > tolerance ||
            slot < 1 || slot > (int32_t) IR_DATA_HALF_BITS ||
            s_frame_hits[slot - 1]) {
            s_frame_timing_invalid = true;
            s_frame_rejected_edges++;
        } else {
            s_frame_hits[slot - 1] = true;
            s_frame_edge_count++;
        }
    }
}

static void ir_receiver_assign_slots(void) {
    static const int32_t slot_offsets[] = {0, -1, 1};
    int32_t count = (int32_t) s_frame_edge_time_count;
    int32_t half_bit = IR_HALF_BIT_X16;
    int32_t tolerance = (int32_t) IR_SLOT_TOLERANCE_SAMPLES * 16;
    int32_t reference = 0;
    int32_t best_votes = 0;
    bool overflow = s_frame_timing_invalid;

    if (count == 0) {
        return;
    }

    // The edge that most others sit on the same half-bit grid as defines the phase.
    for (int32_t j = 0; j < count; j++) {
        int32_t votes = 0;
        for (int32_t i = 0; i < count; i++) {
            int32_t delta = ((int32_t) s_frame_edge_times[i] -
                             (int32_t) s_frame_edge_times[j]) * 16;
            int32_t residual = delta - ir_receiver_round_slots(delta) * half_bit;
            if ((residual < 0 ? -residual : residual) <= tolerance) {
                votes++;
            }
        }
        if (votes > best_votes) {
            best_votes = votes;
            reference = j;
        }
    }

    // Weak START bursts are detected late, so prefer a grid up to 3/4 half-bit after the
    // START edge; neighbouring alignments are accepted only if the frame itself validates.
    int32_t reference_x16 = (int32_t) s_frame_edge_times[reference] * 16;
    int32_t numerator = reference_x16 - half_bit / 4;
    int32_t reference_slot = numerator > 0 ? (numerator + half_bit - 1) / half_bit : 0;

    for (uint k = 0; k < sizeof(slot_offsets) / sizeof(slot_offsets[0]); k++) {
        uint16_t code;
        ir_receiver_fill_slots(reference, reference_slot + slot_offsets[k], overflow);
        if (!s_frame_timing_invalid && ir_receiver_build_code(&code) &&
            ir_receiver_check_parity(code)) {
            return;
        }
    }
    ir_receiver_fill_slots(reference, reference_slot, overflow);
}

static void ir_receiver_finalize_frame(void) {
    uint16_t code = 0;
    uint32_t half_bit_mask = 0;

    ir_receiver_assign_slots();
    bool timing_valid = !s_frame_timing_invalid;

    for (uint slot = 0; slot < IR_DATA_HALF_BITS; slot++) {
        if (s_frame_hits[slot]) {
            half_bit_mask |= 1u << slot;
        }
    }

    if (!ir_receiver_build_code(&code)) {
        timing_valid = false;
    }

    s_frame_collecting = false;
    s_frames_completed++;
    s_last_code = code;
    s_last_half_bit_mask = half_bit_mask;
    s_last_frame_edges = s_frame_edge_count;
    s_last_frame_rejected_edges = s_frame_rejected_edges;

    bool frame_valid = timing_valid && ir_receiver_check_parity(code);
    if (!frame_valid && s_frame_edge_count >= IR_FAILED_FRAME_MIN_EDGES) {
        s_failed_frames++;
        s_failed_half_bit_mask = half_bit_mask;
        s_failed_code = code;
        s_failed_edges = s_frame_edge_count;
        s_failed_rejected_edges = s_frame_rejected_edges;
        uint8_t kept = s_frame_edge_time_count < IR_FAILED_EDGE_TIMES ?
                       s_frame_edge_time_count : IR_FAILED_EDGE_TIMES;
        for (uint i = 0; i < kept; i++) {
            s_failed_edge_times[i] = s_frame_edge_times[i];
        }
        s_failed_edge_time_count = kept;
    }

    if (!timing_valid) {
        s_timing_errors++;
    } else if (!ir_receiver_check_parity(code)) {
        s_parity_errors++;
    } else {
        s_valid_frames++;
        s_last_byte = (uint8_t) code;
        ir_receiver_queue_byte((uint8_t) code);
    }

    s_previous_edge_count = 0;
}

static void ir_receiver_begin_frame(uint32_t edge_sample) {
    memset(s_frame_hits, 0, sizeof(s_frame_hits));
    s_frame_edge_count = 0;
    s_frame_rejected_edges = 0;
    s_frame_edge_time_count = 0;
    s_frame_origin_sample = edge_sample;
    s_frame_timing_invalid = false;
    s_frame_collecting = true;
    s_starts_detected++;
    s_previous_edge_count = 0;
}

static void ir_receiver_process_edge(uint32_t edge_sample) {
    // Valid data never has three consecutive half-bit edges, so this restarts any frame.
    if (s_previous_edge_count >= 2u) {
        uint32_t first_interval = s_previous_edges[1] - s_previous_edges[0];
        uint32_t second_interval = edge_sample - s_previous_edges[1];
        if (ir_receiver_near_half_bit(first_interval) &&
            ir_receiver_near_half_bit(second_interval)) {
            ir_receiver_begin_frame(edge_sample);
            return;
        }
    }

    if (s_previous_edge_count == 0u) {
        s_previous_edges[0] = edge_sample;
        s_previous_edge_count = 1u;
    } else if (s_previous_edge_count == 1u) {
        s_previous_edges[1] = edge_sample;
        s_previous_edge_count = 2u;
    } else {
        s_previous_edges[0] = s_previous_edges[1];
        s_previous_edges[1] = edge_sample;
    }

    if (s_frame_collecting) {
        if (s_frame_edge_time_count < IR_FRAME_MAX_EDGES) {
            s_frame_edge_times[s_frame_edge_time_count++] =
                (uint16_t) (edge_sample - s_frame_origin_sample);
        } else {
            s_frame_timing_invalid = true;
        }
    }
}

static void ir_receiver_process_sample(uint16_t sample) {
    if (!s_baseline_initialized) {
        s_baseline_q8 = (int32_t) sample << 8;
        s_baseline_initialized = true;
        s_sample_index++;
        return;
    }

    int32_t baseline = s_baseline_q8 >> 8;
    int32_t difference = (int32_t) sample - baseline;
    uint16_t magnitude = (uint16_t) (difference < 0 ? -difference : difference);

    if (!s_in_burst) {
        s_baseline_q8 += (((int32_t) sample << 8) - s_baseline_q8) >> IR_BASELINE_SHIFT;
        s_noise_q8 += (((int32_t) magnitude << 8) - s_noise_q8) >> IR_NOISE_SHIFT;
    }

    int16_t delayed = s_carrier_history[s_carrier_history_index];
    s_carrier_history[s_carrier_history_index] = (int16_t) difference;
    s_carrier_history_index = (s_carrier_history_index + 1u) % IR_CARRIER_LAG_SAMPLES;

    int32_t correlation = difference * delayed;
    s_correlation_sum += correlation - s_correlation_window[s_correlation_window_index];
    s_correlation_window[s_correlation_window_index] = correlation;
    uint32_t energy = (uint32_t) (difference * difference);
    s_energy_sum -= s_energy_window[s_correlation_window_index];
    s_energy_sum += energy;
    s_energy_window[s_correlation_window_index] = energy;
    s_correlation_window_index = (s_correlation_window_index + 1u) % IR_CARRIER_WINDOW_SAMPLES;
    if (s_correlation_samples < IR_CARRIER_WINDOW_SAMPLES) {
        s_correlation_samples++;
    }

    bool enough_signal = s_energy_sum >= IR_MIN_SIGNAL_ENERGY;
    bool score_update = (s_sample_index & 7u) == 0u;
    if (score_update && s_correlation_samples == IR_CARRIER_WINDOW_SAMPLES) {
        uint16_t score = 0;
        if (enough_signal && s_correlation_sum > 0) {
            uint64_t scaled_correlation =
                (uint64_t) s_correlation_sum * IR_CARRIER_SCORE_SCALE;
            uint64_t normalized_score = scaled_correlation / s_energy_sum;
            score = (uint16_t) (normalized_score > IR_CARRIER_SCORE_SCALE ?
                                IR_CARRIER_SCORE_SCALE : normalized_score);
        }

        if (s_in_burst) {
            ir_receiver_update_pulse_score(score);
            if (score > s_burst_peak) {
                s_burst_peak = score;
            }
            ir_receiver_update_score_thresholds();
        } else {
            if (s_bursts_detected == 0u && score > s_first_candidate_peak) {
                s_first_candidate_peak = score;
            }
            ir_receiver_update_background_score(score);
        }
    }

    bool carrier_on = s_background_samples >= IR_BACKGROUND_WARMUP_SAMPLES &&
                      enough_signal && s_correlation_sum > 0 &&
                      (uint64_t) s_correlation_sum * IR_CARRIER_SCORE_SCALE >=
                      (uint64_t) s_energy_sum * s_on_threshold;
    bool carrier_off = !enough_signal || s_correlation_sum <= 0 ||
                       (uint64_t) s_correlation_sum * IR_CARRIER_SCORE_SCALE <=
                       (uint64_t) s_energy_sum * s_off_threshold;

    if (s_frame_collecting) {
        uint32_t frame_limit = (IR_DATA_HALF_BITS + 1u) * IR_HALF_BIT_SAMPLES +
                               IR_SLOT_TOLERANCE_SAMPLES;
        if (s_sample_index - s_frame_origin_sample > frame_limit) {
            ir_receiver_finalize_frame();
        }
    }

    if (!s_in_burst && s_correlation_samples == IR_CARRIER_WINDOW_SAMPLES && carrier_on) {
        s_in_burst = true;
        uint64_t score = (uint64_t) s_correlation_sum * IR_CARRIER_SCORE_SCALE /
                         s_energy_sum;
        s_burst_peak = (uint16_t) (score > IR_CARRIER_SCORE_SCALE ? IR_CARRIER_SCORE_SCALE : score);
        ir_receiver_update_pulse_score(s_burst_peak);
        uint32_t interval = s_sample_index - s_last_leading_edge_sample;
        if (!s_have_last_edge || interval >= IR_MIN_EDGE_SPACING_SAMPLES) {
            s_bursts_detected++;
            if (s_have_last_edge) {
                s_last_interval_samples = interval;
            }
            s_last_leading_edge_sample = s_sample_index;
            s_have_last_edge = true;
            ir_receiver_process_edge(s_sample_index);
        }
    } else if (s_in_burst) {
        if (carrier_off) {
            s_in_burst = false;
            s_last_burst_peak = s_burst_peak;
        }
    }

    s_sample_index++;
}

static void ir_receiver_process_dma_block(uint channel_index) {
    const uint16_t *samples = s_dma_samples[channel_index];
    uint16_t minimum = 0x0fffu;
    uint16_t maximum = 0;

    for (uint sample_index = 0; sample_index < IR_DMA_BLOCK_SAMPLES; sample_index++) {
        uint16_t sample = samples[sample_index] & 0x0fffu;
        ir_receiver_process_sample(sample);
        if (sample < minimum) {
            minimum = sample;
        }
        if (sample > maximum) {
            maximum = sample;
        }
    }

    s_last_block_min = minimum;
    s_last_block_max = maximum;
    s_samples_captured += IR_DMA_BLOCK_SAMPLES;
    s_blocks_completed++;

    if ((adc_hw->fcs & ADC_FCS_OVER_BITS) != 0) {
        s_fifo_overruns++;
        ir_receiver_clear_fifo_overrun();
    }

    dma_channel_set_trans_count(s_dma_channels[channel_index], IR_DMA_BLOCK_SAMPLES, false);
}

static void ir_receiver_dma_irq_handler(void) {
    uint32_t pending_channels = dma_hw->ints0 & s_dma_channel_mask;

    for (uint channel_index = 0; channel_index < 2; channel_index++) {
        uint channel = s_dma_channels[channel_index];
        if ((pending_channels & (1u << channel)) != 0) {
            dma_channel_acknowledge_irq0(channel);
            ir_receiver_process_dma_block(channel_index);
        }
    }
}

void ir_receiver_init(void) {
    adc_init();
    adc_gpio_init(IR_RECEIVER_ADC_GPIO);
    adc_select_input(IR_RECEIVER_ADC_INPUT);
    adc_set_round_robin(0);
    adc_set_clkdiv(0.0f);
    adc_fifo_setup(true, true, 1, false, false);
    adc_fifo_drain();
    ir_receiver_clear_fifo_overrun();

    s_samples_captured = 0;
    s_blocks_completed = 0;
    s_fifo_overruns = 0;
    s_last_block_min = 0x0fffu;
    s_last_block_max = 0;
    s_sample_index = 0;
    s_baseline_q8 = 0;
    s_noise_q8 = 0;
    s_baseline_initialized = false;
    memset(s_carrier_history, 0, sizeof(s_carrier_history));
    memset(s_correlation_window, 0, sizeof(s_correlation_window));
    memset(s_energy_window, 0, sizeof(s_energy_window));
    s_correlation_sum = 0;
    s_energy_sum = 0;
    s_carrier_history_index = 0;
    s_correlation_window_index = 0;
    s_correlation_samples = 0;
    s_background_score_q8 = 0;
    s_background_deviation_q8 = 0;
    s_pulse_score_q8 = 0;
    s_background_samples = 0;
    s_background_initialized = false;
    s_pulse_score_initialized = false;
    s_on_threshold = IR_CARRIER_SCORE_SCALE;
    s_off_threshold = 0;
    s_burst_peak = 0;
    s_bursts_detected = 0;
    s_last_leading_edge_sample = 0;
    s_last_interval_samples = 0;
    s_last_burst_peak = 0;
    s_first_candidate_peak = 0;
    s_have_last_edge = false;
    s_in_burst = false;
    memset(s_previous_edges, 0, sizeof(s_previous_edges));
    s_previous_edge_count = 0;
    s_frame_collecting = false;
    s_frame_timing_invalid = false;
    s_frame_origin_sample = 0;
    memset(s_frame_hits, 0, sizeof(s_frame_hits));
    s_frame_edge_count = 0;
    s_frame_rejected_edges = 0;
    s_last_half_bit_mask = 0;
    s_last_frame_edges = 0;
    s_last_frame_rejected_edges = 0;
    s_last_slot = 0;
    s_starts_detected = 0;
    s_frames_completed = 0;
    s_timing_errors = 0;
    s_parity_errors = 0;
    s_valid_frames = 0;
    s_last_code = 0;
    s_last_byte = 0;
    s_failed_frames = 0;
    s_failed_half_bit_mask = 0;
    s_failed_code = 0;
    s_failed_edges = 0;
    s_failed_rejected_edges = 0;
    s_frame_edge_time_count = 0;
    s_failed_edge_time_count = 0;
    memset(s_rx_ring, 0, sizeof(s_rx_ring));
    s_rx_head = 0;
    s_rx_tail = 0;

    s_dma_channels[0] = (uint) dma_claim_unused_channel(true);
    s_dma_channels[1] = (uint) dma_claim_unused_channel(true);
    s_dma_channel_mask = (1u << s_dma_channels[0]) | (1u << s_dma_channels[1]);

    for (uint channel_index = 0; channel_index < 2; channel_index++) {
        uint channel = s_dma_channels[channel_index];
        dma_channel_config config = dma_channel_get_default_config(channel);

        channel_config_set_transfer_data_size(&config, DMA_SIZE_16);
        channel_config_set_read_increment(&config, false);
        channel_config_set_write_increment(&config, true);
        channel_config_set_dreq(&config, DREQ_ADC);
        channel_config_set_chain_to(&config, s_dma_channels[1u - channel_index]);
        channel_config_set_ring(&config, true, IR_DMA_BLOCK_RING_BITS);

        dma_channel_configure(channel,
                              &config,
                              s_dma_samples[channel_index],
                              &adc_hw->fifo,
                              IR_DMA_BLOCK_SAMPLES,
                              false);
        dma_channel_set_irq0_enabled(channel, true);
    }

    irq_set_exclusive_handler(DMA_IRQ_0, ir_receiver_dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    dma_start_channel_mask(1u << s_dma_channels[0]);
    adc_run(true);
}

void ir_receiver_get_acquisition_status(ir_receiver_acquisition_status_t *status) {
    if (!status) {
        return;
    }

    uint32_t interrupt_state = save_and_disable_interrupts();
    status->samples_captured = s_samples_captured;
    status->blocks_completed = s_blocks_completed;
    status->fifo_overruns = s_fifo_overruns;
    status->last_block_min = s_last_block_min;
    status->last_block_max = s_last_block_max;
    restore_interrupts(interrupt_state);
}

void ir_receiver_get_burst_status(ir_receiver_burst_status_t *status) {
    if (!status) {
        return;
    }

    uint32_t interrupt_state = save_and_disable_interrupts();
    status->bursts_detected = s_bursts_detected;
    status->last_leading_edge_sample = s_last_leading_edge_sample;
    status->last_interval_samples = s_last_interval_samples;
    status->baseline = (uint16_t) (s_baseline_q8 >> 8);
    status->noise = (uint16_t) (s_noise_q8 >> 8);
    status->on_threshold = s_on_threshold;
    status->off_threshold = s_off_threshold;
    status->last_burst_peak = s_last_burst_peak;
    status->background_score = (uint16_t) (s_background_score_q8 >> 8);
    status->background_deviation = (uint16_t) (s_background_deviation_q8 >> 8);
    status->pulse_score = (uint16_t) (s_pulse_score_q8 >> 8);
    status->first_candidate_peak = s_first_candidate_peak;
    restore_interrupts(interrupt_state);
}

void ir_receiver_get_decode_status(ir_receiver_decode_status_t *status) {
    if (!status) {
        return;
    }

    uint32_t interrupt_state = save_and_disable_interrupts();
    status->starts_detected = s_starts_detected;
    status->frames_completed = s_frames_completed;
    status->timing_errors = s_timing_errors;
    status->parity_errors = s_parity_errors;
    status->valid_frames = s_valid_frames;
    status->last_code = s_last_code;
    status->last_byte = s_last_byte;
    status->last_half_bit_mask = s_last_half_bit_mask;
    status->last_frame_edges = s_last_frame_edges;
    status->last_frame_rejected_edges = s_last_frame_rejected_edges;
    status->last_slot = s_last_slot;
    restore_interrupts(interrupt_state);
}

void ir_receiver_get_failure_status(ir_receiver_failure_status_t *status) {
    if (!status) {
        return;
    }

    uint32_t interrupt_state = save_and_disable_interrupts();
    status->failed_frames = s_failed_frames;
    status->half_bit_mask = s_failed_half_bit_mask;
    status->code = s_failed_code;
    status->edges = s_failed_edges;
    status->rejected_edges = s_failed_rejected_edges;
    restore_interrupts(interrupt_state);
}

size_t ir_receiver_get_failed_edge_times(uint16_t *times, size_t max_count) {
    if (!times || max_count == 0) {
        return 0;
    }

    uint32_t interrupt_state = save_and_disable_interrupts();
    size_t count = s_failed_edge_time_count;
    if (count > max_count) {
        count = max_count;
    }
    for (size_t i = 0; i < count; i++) {
        times[i] = s_failed_edge_times[i];
    }
    restore_interrupts(interrupt_state);
    return count;
}

size_t ir_receiver_read(uint8_t *buffer, size_t max_count) {
    if (!buffer || max_count == 0) {
        return 0;
    }

    size_t count = 0;
    uint32_t interrupt_state = save_and_disable_interrupts();
    while (count < max_count && s_rx_tail != s_rx_head) {
        buffer[count++] = s_rx_ring[s_rx_tail];
        s_rx_tail = (uint16_t) ((s_rx_tail + 1u) & IR_RX_RING_MASK);
    }
    restore_interrupts(interrupt_state);
    return count;
}