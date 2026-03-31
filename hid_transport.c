#include <string.h>
#include <stdbool.h>

#include "tusb.h"
#include "class/hid/hid_device.h"

#include "command_interface.h"
#include "usb_descriptors.h"
#include "hid_transport.h"

#define REPORT_SIZE USB_HID_REPORT_SIZE

static uint8_t s_pending_response[REPORT_SIZE] = {0};
static uint16_t s_pending_response_len = 0;
static uint8_t s_pending_command[REPORT_SIZE] = {0};
static uint16_t s_pending_command_len = 0;

static void hid_transport_try_flush(void) {
    if (s_pending_response_len == 0) {
        return;
    }

    if (!tud_hid_ready()) {
        return;
    }

    if (tud_hid_report(0, s_pending_response, s_pending_response_len)) {
        s_pending_response_len = 0;
    }
}

static void hid_transport_queue_response(const char *response) {
    memset(s_pending_response, 0, sizeof(s_pending_response));
    if (response) {
        size_t len = strnlen(response, REPORT_SIZE - 1);
        memcpy(s_pending_response, response, len);
    }
    // Always send a full-sized report so the host never sees a short packet
    s_pending_response_len = REPORT_SIZE;
}

void hid_transport_init(void) {
    s_pending_response_len = 0;
    s_pending_command_len = 0;
}

void hid_transport_task(void) {
    if (s_pending_command_len != 0 && s_pending_response_len == 0) {
        char response[REPORT_SIZE] = {0};
        command_interface_process(s_pending_command, s_pending_command_len, response, REPORT_SIZE);
        s_pending_command_len = 0;
        hid_transport_queue_response(response);
    }

    hid_transport_try_flush();
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    (void) instance;
    (void) report_id;

    if (report_type == HID_REPORT_TYPE_OUTPUT) {
        uint16_t copy_len = bufsize;
        if (copy_len > REPORT_SIZE) {
            copy_len = REPORT_SIZE;
        }

        // Keep only one in-flight command. Host should send next command
        // after reading the previous response.
        if (s_pending_command_len == 0 && s_pending_response_len == 0) {
            memset(s_pending_command, 0, sizeof(s_pending_command));
            memcpy(s_pending_command, buffer, copy_len);
            s_pending_command_len = copy_len;
        }
    }
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;

    return 0;
}
