#pragma once
#include <stdint.h>
#define M_PI 3.14159265358979323846
#define M_2PI (2.0 * M_PI)

struct Point{
    long int X;
    long int Y;
    unsigned int XLim;
    unsigned int overX;
};

double modd(double x, double y);
int floor(float x);
int ceil(float x);
float fabs(float x);
float sqrt(float x);
float pow(float x, float y);
float fmod(float x, float y);
double cos(double x);
float sin(float x);
float acos(float x);
float roundf(float x);
uint32_t abs(int number);
int ipart(float x);
float fpart(float x);
float rfpart(float x);
uint64_t min(uint64_t a, uint64_t b);