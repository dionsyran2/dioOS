#pragma once
#include <stdint.h>
#include <stddef.h>

typedef long int time_t;
typedef long int suseconds_t;
typedef long int nanoseconds_t;


struct timeval {
    time_t       tv_sec;   /* Seconds */
    suseconds_t  tv_usec;  /* Microseconds */
};

struct timespec {
    time_t       tv_sec;   /* Seconds */
    nanoseconds_t  tv_nsec;  /* Nanoseconds */
};

struct timezone {
	int tz_minuteswest;
	int tz_dsttime;
};