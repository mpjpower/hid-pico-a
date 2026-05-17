#include "ir_printer.h"

#include "hardware/clocks.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

#include "ir_printer.pio.h"

#define IR_CARRIER_HZ 32768u
#define IR_PIO_CLOCK_HZ (2u * IR_CARRIER_HZ)
#define IR_FRAME_HALFBITS 30u
#define IR_FRAME_US ((IR_FRAME_HALFBITS * 1000000u) / IR_PIO_CLOCK_HZ)

static PIO ir_pio = pio0;
static unsigned int ir_sm = 0;

static unsigned int ir_gpio = IR_PRINTER_GPIO_PIN;
static bool s_burst_active = false;
static absolute_time_t s_burst_deadline;

static const uint32_t BYTE_FRAME_LUT[256] = {
    0xEAAAAAA0, 0xEB4AAAC0, 0xECCAAB20, 0xED2AAB40, 0xED2AACA0, 0xECCAACC0, 0xEB4AAD20, 0xEAAAAD40, 0xF2CAB2A0, 0xF32AB2C0, 0xF4AAB320, 0xF54AB340, 0xF54AB4A0, 0xF4AAB4C0, 0xF32AB520, 0xF2CAB540,
    0xF32ACAA0, 0xF2CACAC0, 0xF54ACB20, 0xF4AACB40, 0xF4AACCA0, 0xF54ACCC0, 0xF2CACD20, 0xF32ACD40, 0xEB4AD2A0, 0xEAAAD2C0, 0xED2AD320, 0xECCAD340, 0xECCAD4A0, 0xED2AD4C0, 0xEAAAD520, 0xEB4AD540,
    0xF4AB2AA0, 0xF54B2AC0, 0xF2CB2B20, 0xF32B2B40, 0xF32B2CA0, 0xF2CB2CC0, 0xF54B2D20, 0xF4AB2D40, 0xECCB32A0, 0xED2B32C0, 0xEAAB3320, 0xEB4B3340, 0xEB4B34A0, 0xEAAB34C0, 0xED2B3520, 0xECCB3540,
    0xED2B4AA0, 0xECCB4AC0, 0xEB4B4B20, 0xEAAB4B40, 0xEAAB4CA0, 0xEB4B4CC0, 0xECCB4D20, 0xED2B4D40, 0xF54B52A0, 0xF4AB52C0, 0xF32B5320, 0xF2CB5340, 0xF2CB54A0, 0xF32B54C0, 0xF4AB5520, 0xF54B5540,
    0xF52CAAA0, 0xF4CCAAC0, 0xF34CAB20, 0xF2ACAB40, 0xF2ACACA0, 0xF34CACC0, 0xF4CCAD20, 0xF52CAD40, 0xED4CB2A0, 0xECACB2C0, 0xEB2CB320, 0xEACCB340, 0xEACCB4A0, 0xEB2CB4C0, 0xECACB520, 0xED4CB540,
    0xECACCAA0, 0xED4CCAC0, 0xEACCCB20, 0xEB2CCB40, 0xEB2CCCA0, 0xEACCCCC0, 0xED4CCD20, 0xECACCD40, 0xF4CCD2A0, 0xF52CD2C0, 0xF2ACD320, 0xF34CD340, 0xF34CD4A0, 0xF2ACD4C0, 0xF52CD520, 0xF4CCD540,
    0xEB2D2AA0, 0xEACD2AC0, 0xED4D2B20, 0xECAD2B40, 0xECAD2CA0, 0xED4D2CC0, 0xEACD2D20, 0xEB2D2D40, 0xF34D32A0, 0xF2AD32C0, 0xF52D3320, 0xF4CD3340, 0xF4CD34A0, 0xF52D34C0, 0xF2AD3520, 0xF34D3540,
    0xF2AD4AA0, 0xF34D4AC0, 0xF4CD4B20, 0xF52D4B40, 0xF52D4CA0, 0xF4CD4CC0, 0xF34D4D20, 0xF2AD4D40, 0xEACD52A0, 0xEB2D52C0, 0xECAD5320, 0xED4D5340, 0xED4D54A0, 0xECAD54C0, 0xEB2D5520, 0xEACD5540,
    0xED52AAA0, 0xECB2AAC0, 0xEB32AB20, 0xEAD2AB40, 0xEAD2ACA0, 0xEB32ACC0, 0xECB2AD20, 0xED52AD40, 0xF532B2A0, 0xF4D2B2C0, 0xF352B320, 0xF2B2B340, 0xF2B2B4A0, 0xF352B4C0, 0xF4D2B520, 0xF532B540,
    0xF4D2CAA0, 0xF532CAC0, 0xF2B2CB20, 0xF352CB40, 0xF352CCA0, 0xF2B2CCC0, 0xF532CD20, 0xF4D2CD40, 0xECB2D2A0, 0xED52D2C0, 0xEAD2D320, 0xEB32D340, 0xEB32D4A0, 0xEAD2D4C0, 0xED52D520, 0xECB2D540,
    0xF3532AA0, 0xF2B32AC0, 0xF5332B20, 0xF4D32B40, 0xF4D32CA0, 0xF5332CC0, 0xF2B32D20, 0xF3532D40, 0xEB3332A0, 0xEAD332C0, 0xED533320, 0xECB33340, 0xECB334A0, 0xED5334C0, 0xEAD33520, 0xEB333540,
    0xEAD34AA0, 0xEB334AC0, 0xECB34B20, 0xED534B40, 0xED534CA0, 0xECB34CC0, 0xEB334D20, 0xEAD34D40, 0xF2B352A0, 0xF35352C0, 0xF4D35320, 0xF5335340, 0xF53354A0, 0xF4D354C0, 0xF3535520, 0xF2B35540,
    0xF2D4AAA0, 0xF334AAC0, 0xF4B4AB20, 0xF554AB40, 0xF554ACA0, 0xF4B4ACC0, 0xF334AD20, 0xF2D4AD40, 0xEAB4B2A0, 0xEB54B2C0, 0xECD4B320, 0xED34B340, 0xED34B4A0, 0xECD4B4C0, 0xEB54B520, 0xEAB4B540,
    0xEB54CAA0, 0xEAB4CAC0, 0xED34CB20, 0xECD4CB40, 0xECD4CCA0, 0xED34CCC0, 0xEAB4CD20, 0xEB54CD40, 0xF334D2A0, 0xF2D4D2C0, 0xF554D320, 0xF4B4D340, 0xF4B4D4A0, 0xF554D4C0, 0xF2D4D520, 0xF334D540,
    0xECD52AA0, 0xED352AC0, 0xEAB52B20, 0xEB552B40, 0xEB552CA0, 0xEAB52CC0, 0xED352D20, 0xECD52D40, 0xF4B532A0, 0xF55532C0, 0xF2D53320, 0xF3353340, 0xF33534A0, 0xF2D534C0, 0xF5553520, 0xF4B53540,
    0xF5554AA0, 0xF4B54AC0, 0xF3354B20, 0xF2D54B40, 0xF2D54CA0, 0xF3354CC0, 0xF4B54D20, 0xF5554D40, 0xED3552A0, 0xECD552C0, 0xEB555320, 0xEAB55340, 0xEAB554A0, 0xEB5554C0, 0xECD55520, 0xED355540
};

void ir_printer_init(unsigned int gpio_pin) {
    float clkdiv;
    unsigned int offset;
    pio_sm_config c;

    ir_gpio = gpio_pin;

    offset = pio_add_program(ir_pio, &ir_printer_program);
    c = ir_printer_program_get_default_config(offset);

    sm_config_set_out_shift(&c, false, true, 30);
    sm_config_set_sideset_pins(&c, ir_gpio);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_NONE);

    clkdiv = (float) clock_get_hz(clk_sys) / (float) IR_PIO_CLOCK_HZ;
    sm_config_set_clkdiv(&c, clkdiv);

    pio_gpio_init(ir_pio, ir_gpio);
    pio_sm_set_consecutive_pindirs(ir_pio, ir_sm, ir_gpio, 1, true);

    pio_sm_init(ir_pio, ir_sm, offset + ir_printer_offset_ir_start, &c);
    pio_sm_set_enabled(ir_pio, ir_sm, true);
}

void send_ir_frame(uint32_t frame) {
    pio_sm_put_blocking(ir_pio, ir_sm, frame);

    // A frame is 27 half-bits plus 3 half-bit gap; TX FIFO write is immediate.
    busy_wait_us(IR_FRAME_US);
}

void ir_printer_send_byte(uint8_t c) {
    send_ir_frame(BYTE_FRAME_LUT[c]);
}

void ir_printer_send_bytes(const uint8_t *data, size_t len) {
    if (!data || len == 0) {
        return;
    }

    for (size_t i = 0; i < len; i++) {
        ir_printer_send_byte(data[i]);
    }
}

void ir_printer_send_carrier_burst_ms(uint32_t duration_ms) {
    uint64_t elapsed_us;
    uint64_t target_us;
    uint64_t frame_total_us;

    if (duration_ms == 0u) {
        return;
    }

    elapsed_us = 0u;
    target_us = (uint64_t) duration_ms * 1000u;
    frame_total_us = (uint64_t) IR_FRAME_US;

    while (elapsed_us < target_us) {
        send_ir_frame(0xFFFFFFFFu);
        elapsed_us += frame_total_us;
    }
}

void ir_printer_start_carrier_burst_ms(uint32_t duration_ms) {
    if (duration_ms == 0u) {
        s_burst_active = false;
        return;
    }

    s_burst_deadline = make_timeout_time_ms(duration_ms);
    s_burst_active = true;
}

void ir_printer_task(void) {
    if (!s_burst_active) {
        return;
    }

    if (absolute_time_diff_us(get_absolute_time(), s_burst_deadline) <= 0) {
        s_burst_active = false;
        return;
    }

    send_ir_frame(0xFFFFFFFFu);
}
