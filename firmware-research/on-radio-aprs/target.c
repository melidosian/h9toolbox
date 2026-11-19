#include "update.h"
#include "stage-config.h"

#define BYTE_AT(address)  (*(volatile uint8_t *)(uintptr_t)(address))
#define HALF_AT(address)  (*(volatile uint16_t *)(uintptr_t)(address))
#define WORD_AT(address)  (*(volatile uint32_t *)(uintptr_t)(address))
#define UPDATE_MODE       BYTE_AT(APRS_UPDATE_STATE)
#define BLOCK_ACKS        BYTE_AT(APRS_UPDATE_STATE + 1)
#define MODEM_READY       BYTE_AT(0x11b32)
#define KEY               BYTE_AT(0xc3af)
#define RING_PRODUCER     HALF_AT(0x16e34)
#define RING_BYTE(index)  BYTE_AT(0x16634 + (index))
#define MODEM_UART        WORD_AT(0x11cfc)
#define UART1_OBJECT      0x1e32cu
#define UART_RX_PRODUCED  WORD_AT(0x1e358)

enum {
    MODE_WRITING = 1,
    MODE_FAILED = 2,
    MODE_LAUNCHED = 3,
    MODE_BAD_STAGE = 4,
    POLL_DELAY_MS = 10,
    STARTUP_QUIET_MS = 20,
    ACK_TIMEOUT_MS = 3000
};

void h9_clear(void);
void h9_line(const char *text, unsigned line_y);
void h9_delay(unsigned milliseconds);
void h9_scan_keys(void);
void h9_jl(unsigned command, unsigned length, const uint8_t *payload);
void h9_uart_write(const uint8_t *frame, unsigned length);
void h9_launch(void);
void h9_run_baud(void);

static void status(const char *heading, const char *detail)
{
    h9_clear();
    h9_line(heading, 24);
    h9_line(detail, 52);
}

static int transact(uint8_t command, unsigned size, const uint8_t *payload)
{
    const uint8_t prev = BLOCK_ACKS;
    if (command == APRS_EXIT)
        MODEM_READY = 0;
    h9_jl(command, size, payload);
    for (unsigned elapsed = 0; elapsed < ACK_TIMEOUT_MS; elapsed += POLL_DELAY_MS) {
        if (command == APRS_EXIT ? MODEM_READY != 0 : BLOCK_ACKS != prev)
            return 1;
        h9_delay(POLL_DELAY_MS);
    }
    return 0;
}

static int verify_boot(void)
{
    // The stock bulk writer passes this buffer to UART DMA.
    uint8_t query[] = {0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0x20, 0, 0xc0, 0xc0, 0xc0};
    uint16_t cursor = RING_PRODUCER & 2047;
    struct aprs_version parser = {0, 0};
    for (unsigned attempt = 0; attempt < 4; ++attempt) {
        h9_uart_write(query, sizeof(query));
        for (unsigned elapsed = 0; elapsed < 750; elapsed += POLL_DELAY_MS) {
            const uint16_t end = RING_PRODUCER & 2047;
            for (; cursor != end; cursor = (cursor + 1) & 2047) {
                if (aprs_version_feed(&parser, RING_BYTE(cursor)))
                    return 1;
            }
            h9_delay(POLL_DELAY_MS);
        }
    }
    return 0;
}

static int wait_for_quiet(void)
{
    // Align the first sample with the OS tick before measuring a quiet interval.
    h9_delay(POLL_DELAY_MS);
    uint32_t previous = UART_RX_PRODUCED;
    unsigned quiet = 0;
    for (unsigned elapsed = 0; elapsed < ACK_TIMEOUT_MS; elapsed += POLL_DELAY_MS) {
        h9_delay(POLL_DELAY_MS);
        const uint32_t current = UART_RX_PRODUCED;
        quiet = current == previous ? quiet + POLL_DELAY_MS : 0;
        if (quiet >= STARTUP_QUIET_MS)
            return 1;
        previous = current;
    }
    return 0;
}

static int confirm_update(void)
{
    status("APRS STAGE READY", "Menu writes / Back");
    unsigned released = 0;
    unsigned pressed = 0;
    for (unsigned elapsed = 0; elapsed < 30000; elapsed += POLL_DELAY_MS) {
        h9_delay(POLL_DELAY_MS);
        h9_scan_keys();
        const uint8_t key = KEY;
        if (key == 15)
            return 0;
        if (released && key == 12) {
            if (++pressed == 2)
                return 1;
        } else {
            pressed = 0;
        }
        if (key == 0)
            released = 1;
    }
    return 0;
}

__attribute__((section(".text.entry")))
void aprs_stage_entry(void)
{
    const uint32_t uart = MODEM_UART;
    if (uart != UART1_OBJECT || WORD_AT(uart + 20) == 0 || WORD_AT(uart + 24) == 0) {
        UPDATE_MODE = MODE_BAD_STAGE;
        status("MODEM UNAVAILABLE", "Press Back");
        return;
    }
    const uint8_t *image = (const uint8_t *)(uintptr_t)APRS_STAGE_IMAGE;
    if (aprs_crc32(image, APRS_IMAGE_SIZE) != APRS_STAGE_CRC32) {
        UPDATE_MODE = MODE_BAD_STAGE;
        status("STAGED IMAGE BAD", "Press Back");
        return;
    }
    if (!confirm_update()) {
        UPDATE_MODE = MODE_BAD_STAGE;
        status("UPDATE CANCELLED", "Press Back");
        return;
    }
    if (!wait_for_quiet()) {
        UPDATE_MODE = MODE_BAD_STAGE;
        status("MODEM UART BUSY", "Press Back");
        return;
    }

    status("APRS UPDATE", "Keep power on");
    MODEM_READY = 0;
    BLOCK_ACKS = 0;
    UPDATE_MODE = MODE_WRITING;
    const enum aprs_result result = aprs_program(image, APRS_STAGE_CRC32, transact);
    if (result == APRS_BAD_IMAGE) {
        UPDATE_MODE = MODE_BAD_STAGE;
        status("STAGED IMAGE BAD", "Press Back");
        return;
    }
    if (result != APRS_OK) {
        UPDATE_MODE = MODE_FAILED;
        status(result == APRS_BLOCK_TIMEOUT ? "NO BLOCK ACK" : "NO EXIT ACK", "Recover on Windows");
        return;
    }

    UPDATE_MODE = MODE_LAUNCHED;
    status("CHECKING APRS BOOT", "Keep power on");
    h9_delay(2000);
    h9_launch();
    h9_run_baud();
    h9_delay(300);
    if (verify_boot())
        status("APRS BOOT VERIFIED", APRS_MODEM_VERSION);
    else
        status("APRS NOT VERIFIED", "Check APRS Info");
    h9_delay(2500);
    MODEM_READY = 0;
    KEY = 15;
}
