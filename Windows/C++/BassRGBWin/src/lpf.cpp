#define _USE_MATH_DEFINES
#include "lpf.h"
#include <cmath>

LowPassFilter::LowPassFilter(double sampleRate, double cutoffFreq) {
    double omega = 2.0 * M_PI * cutoffFreq / sampleRate;
    double sinOmega = sin(omega);
    double cosOmega = cos(omega);
    double alpha = sinOmega / 2.0;

    double b0 = (1 - cosOmega) / 2;
    double b1 = 1 - cosOmega;
    double b2 = (1 - cosOmega) / 2;
    double a0 = 1 + alpha;
    double a1 = -2 * cosOmega;
    double a2 = 1 - alpha;

    this->a0 = b0 / a0;
    this->a1 = b1 / a0;
    this->a2 = b2 / a0;
    this->b1 = a1 / a0;
    this->b2 = a2 / a0;
}

double LowPassFilter::process(double input) {
    double output = a0 * input + a1 * z1 + a2 * z2 - b1 * z1 - b2 * z2;
    z2 = z1;
    z1 = output;
    return output;
}
