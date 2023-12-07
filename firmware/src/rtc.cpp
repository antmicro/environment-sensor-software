#include "rtc.h"

#include <stm32_ll_pwr.h>
#include <stm32_ll_rcc.h>
#include <stm32_ll_rtc.h>
#include <zephyr/arch/common/sys_bitops.h>
#include <zephyr/drivers/rtc.h>
#include <zephyr/init.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/timeutil.h>

#include <cstdio>

namespace rtc {
const device* const rtc = DEVICE_DT_GET_ONE(st_stm32_rtc);

// reset rtc domain to allow modification of RTCSEL by rtc driver and enable LSE
static int allow_rtc_lse_selection() {
    // will be triggered only at first boot (if RTCSEL wasn't already LSE) and at boot after removing the battery
    if (LL_RCC_GetRTCClockSource() != LL_RCC_RTC_CLKSOURCE_LSE) {
        LL_PWR_EnableBkUpAccess();
        LL_RCC_ForceBackupDomainReset();
        LL_RCC_ReleaseBackupDomainReset();
        LL_RCC_LSE_Enable();
        while (!LL_RCC_LSE_IsReady()) {
            // wait until LSE is ready
        }
    }

    return 0;
}

SYS_INIT(allow_rtc_lse_selection, POST_KERNEL, 49); // invoke before rtc driver initialization (POST_KERNEL 50)

void init() {
    if (!device_is_ready(rtc)) {
        printk("warning: rtc: not ready\n");
        return;
    }
}

time_t get_current_time() {
    rtc_time dt{};
    rtc_get_time(rtc, &dt);

    return timeutil_timegm64(rtc_time_to_tm(&dt));
}

void print_time(time_t timestamp) {
    tm* tm = gmtime(&timestamp);

    printk("%04d-%02d-%02dT%02d:%02d:%02d",
           tm->tm_year + 1900,
           tm->tm_mon + 1,
           tm->tm_mday,
           tm->tm_hour,
           tm->tm_min,
           tm->tm_sec);
}

int set_current_time(const char* timestamp) {
    rtc_time dt{};

    sscanf(timestamp,
           "%04d-%02d-%02dT%02d:%02d:%02d",
           &dt.tm_year,
           &dt.tm_mon,
           &dt.tm_mday,
           &dt.tm_hour,
           &dt.tm_min,
           &dt.tm_sec);

    dt.tm_year -= 1900;
    dt.tm_mon -= 1;

    return rtc_set_time(rtc, &dt);
}
}