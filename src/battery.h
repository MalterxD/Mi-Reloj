#ifndef BATTERY_H
#define BATTERY_H

typedef struct {
    int percentage;
    char status[32];
} BatteryInfo;

BatteryInfo get_battery_info(void);

#endif
