/* Preliminary motion-signal features from an earlier local monitoring prototype.
 * Executable logic is unchanged; historical comments have been clarified.
 * Configured sample rate: 208 Hz. Saved tests use 624 signed samples (3 s).
 * Each call reports spectral features and a single-window detection decision.
 * Thresholds are exploratory constants, not validated clinical limits.
 * No sensor acquisition, stimulation, display or consecutive-window hold here.
 * The saved tests do not validate vector-magnitude preprocessing.
 */

#include <math.h>
#include <string.h>
#include "features.h"

#define FS        208.0f
#define NBIN      40            
#define BIN_HZ    0.5f

static float goertzel(const float *x, int n, float fhz) {
    float w = 2.0f*(float)M_PI*fhz/FS;
    float c = 2.0f*cosf(w), s1=0, s2=0;
    for (int i=0;i<n;i++){ float s0 = x[i] + c*s1 - s2; s2=s1; s1=s0; }
    return s1*s1 + s2*s2 - c*s1*s2;     
}

void vm_features(const float *gyro_mag, int n, vm_feat_t *out) {
    float p[NBIN]; float total=0;
    for (int k=0;k<NBIN;k++){ p[k]=goertzel(gyro_mag,n,(k+1)*BIN_HZ); total+=p[k]; }

    
    float p_low=0,p_trem=0,p_hi=0;
    for (int k=0;k<NBIN;k++){
        float f=(k+1)*BIN_HZ;
        if (f>=0.3f && f<3.0f)  p_low += p[k];
        if (f>=3.0f && f<9.0f)  p_trem+= p[k];
        if (f>=7.0f && f<12.0f) p_hi  += p[k];
    }
    
    int kbest=5; float best=0; float mean_band=0; int cnt=0;
    for (int k=0;k<NBIN;k++){ float f=(k+1)*BIN_HZ;
        if (f>=3.0f && f<=12.0f){ mean_band+=p[k]; cnt++; if(p[k]>best){best=p[k];kbest=k;} } }
    mean_band = cnt? mean_band/cnt : 1e-9f;

    out->dom_freq   = (kbest+1)*BIN_HZ;
    out->p_low      = p_low;
    out->p_tremor   = p_trem;
    out->p_high     = p_hi;
    out->ratio      = p_low>1e-9f ? p_trem/p_low : 0.0f;
    out->prominence = mean_band>1e-9f ? best/mean_band : 0.0f;
    out->total_pwr  = total;
    
    float pb=0; for (int k=0;k<NBIN;k++){ float f=(k+1)*BIN_HZ;
        if (fabsf(f-out->dom_freq)<=1.2f) pb+=p[k]; }
    out->tremor_amp = sqrtf(pb*2.0f/(float)(n*n)) * 1.0f; 
}


int vm_presence_gate(const vm_feat_t *f, float gyro_rms_dps) {
    const float RATIO_TH = 0.30f;   
    const float RMS_TH   = 5.0f;    
    int inband = (f->dom_freq>=3.0f && f->dom_freq<=12.0f);
    (void)gyro_rms_dps; 
    return inband && (f->ratio>RATIO_TH) && (f->tremor_amp>RMS_TH) && (f->prominence>3.0f);
}
