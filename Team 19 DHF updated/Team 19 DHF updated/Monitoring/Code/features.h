#ifndef VM_FEATURES_H
#define VM_FEATURES_H
typedef struct {
    float dom_freq, p_low, p_tremor, p_high, ratio, prominence, total_pwr, tremor_amp;
} vm_feat_t;
void vm_features(const float *gyro_mag, int n, vm_feat_t *out);
int  vm_presence_gate(const vm_feat_t *f, float gyro_rms_dps);
#endif
