#ifndef LPF_H
#define LPF_H

class LowPassFilter {
public:
    LowPassFilter(double sampleRate, double cutoffFreq);
    double process(double input);

private:
    double a0, a1, a2, b1, b2;
    double z1 = 0, z2 = 0;
};

#endif
