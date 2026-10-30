// The few functions of the C library that the demo needs and the firmware's
// ABI does not give (device build only): sin, cos, lround.
#ifndef HOST_BUILD
#include <stdint.h>

namespace
{
    constexpr double PI_2 = 1.57079632679489661923;
    constexpr double PI_2_HI = 1.57079632673412561417e+00; // pi/2 in two parts (Cody-Waite)
    constexpr double PI_2_LO = 6.07710050650619224932e-11;

    // |x| <= pi/4
    double ksin(double x)
    {
        double z = x * x;
        return x + x * z * (-1.0 / 6 + z * (1.0 / 120 + z * (-1.0 / 5040 + z * (1.0 / 362880 + z * (-1.0 / 39916800 + z * (1.0 / 6227020800.0 + z * (-1.0 / 1307674368000.0)))))));
    }

    double kcos(double x)
    {
        double z = x * x;
        return 1.0 + z * (-0.5 + z * (1.0 / 24 + z * (-1.0 / 720 + z * (1.0 / 40320 + z * (-1.0 / 3628800 + z * (1.0 / 479001600.0 + z * (-1.0 / 87178291200.0)))))));
    }

    // x = n * pi/2 + r, |r| <= pi/4
    double reduce(double x, int & n)
    {
        double k = x / PI_2;
        k = k >= 0 ? (double)(int64_t)(k + 0.5) : (double)(int64_t)(k - 0.5);
        n = (int)((int64_t)k & 3);
        return (x - k * PI_2_HI) - k * PI_2_LO;
    }
}

extern "C" double sin(double x)
{
    int n;
    double r = reduce(x, n);
    switch (n)
    {
        case 0: return ksin(r);
        case 1: return kcos(r);
        case 2: return -ksin(r);
        default: return -kcos(r);
    }
}

extern "C" double cos(double x)
{
    int n;
    double r = reduce(x, n);
    switch (n)
    {
        case 0: return kcos(r);
        case 1: return -ksin(r);
        case 2: return -kcos(r);
        default: return ksin(r);
    }
}

extern "C" long lround(double x)
{
    return x >= 0 ? (long)(x + 0.5) : -(long)(-x + 0.5);
}
#endif
