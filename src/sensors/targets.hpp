#ifndef TARGETS_H
#define TARGETS_H

#include <time.h>
struct TargetValues {
    float airHumidity;
    float vpd;
    float soilHumidity;
    float temperature;
    struct tm lightOnTime;
    struct tm lightOffTime;
};
extern TargetValues target;
#endif