#pragma once
#include <stdint.h>


#define M_PI 3.14159265358979323846
#define M_2PI 2 * M_PI
#define modd(x, y) ((x) - (int)((x) / (y)) * (y))


int floor(float x);

int ceil(float x);

float fabs(float x);

float sqrt(float x);

float pow(float x, float y);

float fmod(float x, float y);

double cos(double x);

float acos(float x);

float sin(float x);

float roundf(float x);

int ipart(float x);

float fpart(float x);

float rfpart(float x);

uint32_t abs(int number);

template<typename T>
void swap(T& a, T& b);
//template void swap<float>(float&, float&);