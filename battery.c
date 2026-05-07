#include "battery.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <dirent.h>
#include <unistd.h>

BatteryInfo get_battery_info(void) {
    static BatteryInfo cached_info = {0, "Unknown"};
    static char battery_path[1024] = "";
    static time_t last_update = 0;
    time_t now = time(NULL);

    if (now - last_update < 5 && last_update != 0) {
        return cached_info;
    }

    if (battery_path[0] == '\0') {
        DIR *dir = opendir("/sys/class/power_supply");
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.') continue;
                char type_path[1100], type_buf[32];
                snprintf(type_path, sizeof(type_path), "/sys/class/power_supply/%s/type", entry->d_name);
                FILE *f_type = fopen(type_path, "r");
                if (f_type) {
                    if (fgets(type_buf, sizeof(type_buf), f_type)) {
                        char *nl = strchr(type_buf, '\n');
                        if (nl) *nl = '\0';
                        if (strcmp(type_buf, "Battery") == 0) {
                            snprintf(battery_path, sizeof(battery_path), "/sys/class/power_supply/%s", entry->d_name);
                            fclose(f_type);
                            break;
                        }
                    }
                    fclose(f_type);
                }
            }
            closedir(dir);
        }
    }

    if (battery_path[0] != '\0') {
        char path[1100];
        int read_success = 0;

        snprintf(path, sizeof(path), "%s/capacity", battery_path);
        FILE *f_cap = fopen(path, "r");
        if (f_cap) {
            if (fscanf(f_cap, "%d", &cached_info.percentage) == 1) {
                read_success = 1;
            }
            fclose(f_cap);
        }

        snprintf(path, sizeof(path), "%s/status", battery_path);
        FILE *f_stat = fopen(path, "r");
        if (f_stat) {
            if (fgets(cached_info.status, sizeof(cached_info.status), f_stat)) {
                char *nl = strchr(cached_info.status, '\n');
                if (nl) *nl = '\0';
            } else {
                strcpy(cached_info.status, "Unknown");
            }
            fclose(f_stat);
        } else {
            strcpy(cached_info.status, "Unknown");
        }

        if (!read_success) {
            cached_info.percentage = 0;
            strcpy(cached_info.status, "Unknown");
        }
    } else {
        cached_info.percentage = 0;
        strcpy(cached_info.status, "Unknown");
    }

    last_update = now;
    return cached_info;
}
