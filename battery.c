#include "battery.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

BatteryInfo get_battery_info(void) {
    static BatteryInfo cached_info = {0, "Unknown"};
    static time_t last_update = 0;
    time_t now = time(NULL);

    if (now - last_update < 5 && last_update != 0) {
        return cached_info;
    }

    FILE *f_cap = fopen("/sys/class/power_supply/BAT0/capacity", "r");
    if (f_cap) {
        if (fscanf(f_cap, "%d", &cached_info.percentage) != 1) cached_info.percentage = 0;
        fclose(f_cap);
    }

    FILE *f_stat = fopen("/sys/class/power_supply/BAT0/status", "r");
    if (f_stat) {
        if (fgets(cached_info.status, sizeof(cached_info.status), f_stat)) {
            char *nl = strchr(cached_info.status, '\n');
            if (nl) *nl = '\0';
        }
        fclose(f_stat);
    }

    last_update = now;
    return cached_info;
}
