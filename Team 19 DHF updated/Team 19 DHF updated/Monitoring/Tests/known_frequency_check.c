#include <math.h>
#include <stdio.h>
#include "features.h"

/* Synthetic function-level characterization, not a sensor/patient test.
 * Feed signed sine waves directly to the unchanged feature function, matching
 * the existing host-test interface. No magnitude conversion, noise or drift.
 * Frequencies and phases are fixed before inspecting results; do not tune gates.
 */
static void run_case(const char *kind, double hz, double amplitude, double phase) {
    const double pi = acos(-1.0), fs = 208.0;
    float samples[624];
    double sum_sq = 0.0;
    for (int i = 0; i < 624; ++i) {
        samples[i] = (float)(amplitude * sin(2.0 * pi * hz * i / fs + phase));
        sum_sq += (double)samples[i] * samples[i];
    }
    vm_feat_t feature;
    vm_features(samples, 624, &feature);
    int gate = vm_presence_gate(&feature, (float)sqrt(sum_sq / 624.0));
    printf("%s,%.2f,%.2f,%.9f,624,208,%.3f,%.3f,%.6f,%.6f,%.6f,%d\n",
           kind, hz, amplitude, phase, feature.dom_freq,
           fabs(feature.dom_freq - hz), feature.ratio,
           feature.prominence, feature.tremor_amp, gate);
}

int main(void) {
    const double frequencies[] = {4.0, 5.0, 6.0, 4.25, 5.75};
    const double pi = acos(-1.0);
    const double phases[] = {0.0, pi / 4.0, pi / 2.0};
    puts("case,input_hz,peak_amplitude_dps,phase_rad,samples,sample_rate_hz,estimated_hz,absolute_error_hz,ratio,prominence,band_amplitude,gate");
    for (int f = 0; f < 5; ++f)
        for (int p = 0; p < 3; ++p)
            run_case("in_band_sine", frequencies[f], 20.0, phases[p]);
    run_case("zero_control", 0.0, 0.0, 0.0);
    run_case("slow_sine_control", 1.2, 20.0, 0.0);
    return 0;
}
