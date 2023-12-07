#ifndef ANTENVSENS_RTC_H
#define ANTENVSENS_RTC_H
#include <ctime>

namespace rtc {
void init();
int set_current_time(const char* timestamp);
time_t get_current_time();
void print_time(time_t timestamp);
}

#endif