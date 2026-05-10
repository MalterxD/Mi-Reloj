#include "battery.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <dirent.h>
#include <unistd.h>
#include <stdbool.h>

static bool read_sys_file(const char *base_path, const char *filename, char *buf, size_t size) {
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s/%s", base_path, filename);
    FILE *f = fopen(full_path, "r");
    if (!f) return false;
    if (fgets(buf, size, f)) {
        char *nl = strchr(buf, '\n');
        if (nl) *nl = '\0';
        fclose(f);
        return true;
    }
    fclose(f);
    return false;
}

BatteryInfo get_battery_info(void) {
    static BatteryInfo cached_info = {0, "Unknown"};
    static char battery_path[512] = "";
    static time_t last_update = 0;
    static bool no_battery = false;
    time_t now = time(NULL);

    if (no_battery) {
        return cached_info;
    }

    if (last_update != 0 && now - last_update < 5) {
        return cached_info;
    }

    if (battery_path[0] == '\0') {
        DIR *dir = opendir("/sys/class/power_supply");
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.') continue;
                char base[512], type[32];
                snprintf(base, sizeof(base), "/sys/class/power_supply/%s", entry->d_name);
                if (read_sys_file(base, "type", type, sizeof(type)) && strcmp(type, "Battery") == 0) {
                    snprintf(battery_path, sizeof(battery_path), "%s", base);
                    break;
                }
            }
            closedir(dir);
        }

        if (battery_path[0] == '\0') {
            no_battery = true;
            return cached_info;
        }
    }

    if (battery_path[0] != '\0') {
        char cap_buf[16];
        if (read_sys_file(battery_path, "capacity", cap_buf, sizeof(cap_buf))) {
            cached_info.percentage = atoi(cap_buf);
        } else {
            cached_info.percentage = 0;
        }

        if (!read_sys_file(battery_path, "status", cached_info.status, sizeof(cached_info.status))) {
            strcpy(cached_info.status, "Unknown");
        }
    }

    last_update = now;
    return cached_info;
}
