#pragma once
#include <stdint.h>

/* Boot-time CMOS/RTC snapshot, exposed as Unix seconds. */
void rtc_init(void);
uint32_t rtc_unix_seconds(void);
