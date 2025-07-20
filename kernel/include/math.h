#pragma once
#include <stdint.h>


struct Point{
    long int X;
    long int Y;
    unsigned int XLim;
    unsigned int overX;
};

uint32_t MIN(uint32_t a, uint32_t b);
double pow(double base, int exp);