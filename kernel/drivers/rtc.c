#include <rtc.h>
#include <io.h>
#include <process.h>
#include <stdint.h>

#define CMOS_ADDR 0x70
#define CMOS_DATA  0x71

static uint32_t g_unix_base = 0;
static uint32_t g_tick_base = 0;

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDR, (uint8_t)(reg | 0x80u));
    return inb(CMOS_DATA);
}

static uint8_t bcd_to_bin(uint8_t v) {
    return (uint8_t)((v & 0x0Fu) + ((v >> 4) * 10u));
}

static int64_t days_from_civil(int y, unsigned m, unsigned d) {
    y -= (m <= 2);
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned adj = (m > 2) ? (m - 3u) : (m + 9u);
    const unsigned doy = (153u * adj + 2u) / 5u + d - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

static int century_is_plausible(uint8_t century) {
    /* BIOS-era CMOS century registers are usually 19xx or 20xx. */
    return century >= 19u && century <= 21u;
}

static uint32_t read_cmos_unix(void) {
    uint8_t sec, min, hour, day, month, year, century;
    uint8_t regb;

    for (;;) {
        while (cmos_read(0x0A) & 0x80u) {
            /* wait for update-in-progress to clear */
        }

        sec     = cmos_read(0x00);
        min     = cmos_read(0x02);
        hour    = cmos_read(0x04);
        day     = cmos_read(0x07);
        month   = cmos_read(0x08);
        year    = cmos_read(0x09);
        century = cmos_read(0x32);
        regb    = cmos_read(0x0B);

        if (!(cmos_read(0x0A) & 0x80u)) {
            break;
        }
    }

    if (!(regb & 0x04u)) {
        sec     = bcd_to_bin(sec);
        min     = bcd_to_bin(min);
        hour    = bcd_to_bin(hour);
        day     = bcd_to_bin(day);
        month   = bcd_to_bin(month);
        year    = bcd_to_bin(year);
        century = bcd_to_bin(century);
    }

    if (!(regb & 0x02u)) {
        int pm = hour & 0x80u;
        hour &= 0x7Fu;
        if (pm && hour < 12u) hour = (uint8_t)(hour + 12u);
        if (!pm && hour == 12u) hour = 0;
    }

    int full_year;
    if (century_is_plausible(century)) {
        full_year = (int)century * 100 + (int)year;
    } else if (year < 70u) {
        full_year = 2000 + (int)year;
    } else {
        full_year = 1900 + (int)year;
    }

    int64_t days = days_from_civil(full_year, month, day);
    int64_t unix_seconds = days * 86400 + (int64_t)hour * 3600
        + (int64_t)min * 60 + (int64_t)sec;
    if (unix_seconds < 0) return 0;
    if (unix_seconds > 0xFFFFFFFFLL) return 0xFFFFFFFFu;
    return (uint32_t)unix_seconds;
}

void rtc_init(void) {
    g_unix_base = read_cmos_unix();
    g_tick_base = process_ticks();
}

uint32_t rtc_unix_seconds(void) {
    uint32_t ticks = process_ticks() - g_tick_base;
    uint64_t now = (uint64_t)g_unix_base + (uint64_t)(ticks / 100u);
    if (now > 0xFFFFFFFFu) return 0xFFFFFFFFu;
    return (uint32_t)now;
}
