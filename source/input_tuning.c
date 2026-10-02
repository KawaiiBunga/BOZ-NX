#include "input_tuning.h"
int aim_percent(int percent) {
    return percent < AIM_PCT_MIN ? AIM_PCT_MIN :
           percent > AIM_PCT_MAX ? AIM_PCT_MAX : percent;
}
float aim_axis(float value, int percent) {
    return value * ((float)aim_percent(percent) / 100.0f);
}
