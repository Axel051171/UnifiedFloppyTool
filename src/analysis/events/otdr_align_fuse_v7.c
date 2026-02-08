#include "otdr_event_core_v7.h"
#include "otdr_common.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static float dotf(const float *a, const float *b, size_t n){
    double s=0.0;
    for(size_t i=0;i<n;i++) s += (double)a[i]*(double)b[i];
    return (float)s;
}

static float norm2f(const float *a, size_t n){
    double s=0.0;
    for(size_t i=0;i<n;i++){ double v=a[i]; s += v*v; }
    return (float)sqrt(s);
}

int otdr_estimate_shift_ncc(const float *ref, const float *x, size_t n, int max_lag, float *out_score){
    if(!ref||!x||n==0||max_lag<0) return 0;
    float refn = norm2f(ref,n);
    float xn = norm2f(x,n);
    float denom = refn*xn;
    if(denom <= 1e-20f) denom = 1e-20f;

    int best_shift=0;
    float best=-1e9f;

    for(int lag=-max_lag; lag<=max_lag; lag++){
        /* compute NCC on overlap */
        size_t a0=0, b0=0, len=n;
        if(lag>0){
            a0=(size_t)lag; b0=0; len = (a0<n)?(n-a0):0;
        }else if(lag<0){
            b0=(size_t)(-lag); a0=0; len = (b0<n)?(n-b0):0;
        }
        if(len<16) continue;

        double s=0.0;
        for(size_t i=0;i<len;i++){
            s += (double)ref[a0+i]*(double)x[b0+i];
        }
        float ncc = (float)(s/ (double)denom);
        if(ncc>best){ best=ncc; best_shift=lag; }
    }

    if(out_score) *out_score = best;
    /* shift returned should move target to align with ref: apply +best_shift to target */
    return best_shift;
}

void otdr_apply_shift_zeropad(const float *x, size_t n, int shift, float *out){
    if(!x||!out||n==0) return;
    for(size_t i=0;i<n;i++){
        long src = (long)i - (long)shift;
        if(src<0 || src>=(long)n) out[i]=0.0f;
        else out[i]=x[(size_t)src];
    }
}

int otdr_align_traces(const float *const *traces, size_t m, size_t n, size_t ref_idx,
                      int max_lag, int *shifts_out, float **aligned_out)
{
    if(!traces||!aligned_out||m==0||n==0||ref_idx>=m) return -1;
    const float *ref = traces[ref_idx];

    for(size_t k=0;k<m;k++){
        int sh=0;
        if(k!=ref_idx){
            sh = otdr_estimate_shift_ncc(ref, traces[k], n, max_lag, NULL);
        }
        if(shifts_out) shifts_out[k]=sh;
        otdr_apply_shift_zeropad(traces[k], n, sh, aligned_out[k]);
    }
    return 0;
}

int otdr_fuse_aligned_median(float *const *aligned, size_t m, size_t n, float *out){
    if(!aligned||!out||m==0||n==0) return -1;
    float *tmp=(float*)malloc(m*sizeof(float));
    if(!tmp) return -2;
    for(size_t i=0;i<n;i++){
        for(size_t k=0;k<m;k++) tmp[k]=aligned[k][i];
        qsort(tmp,m,sizeof(float),otdr_cmp_float_asc);
        out[i]=(m&1u)?tmp[m/2]:0.5f*(tmp[m/2-1]+tmp[m/2]);
    }
    free(tmp);
    return 0;
}

int otdr_label_stability(const uint8_t *const *labels, size_t m, size_t n, uint8_t C,
                         float *agree_ratio, float *entropy_like)
{
    if(!labels||m==0||n==0||C==0||!agree_ratio||!entropy_like) return -1;

    unsigned *cnt=(unsigned*)malloc(C*sizeof(unsigned));
    if(!cnt) return -2;

    for(size_t i=0;i<n;i++){
        for(uint8_t c=0;c<C;c++) cnt[c]=0;
        for(size_t k=0;k<m;k++){
            uint8_t lab=labels[k][i];
            if(lab>=C) lab=0;
            cnt[lab]++;
        }
        unsigned best=0;
        double sum_p2=0.0;
        for(uint8_t c=0;c<C;c++){
            if(cnt[c]>best) best=cnt[c];
            double p=(double)cnt[c]/(double)m;
            sum_p2 += p*p;
        }
        agree_ratio[i]=(float)((double)best/(double)m);
        entropy_like[i]=(float)(1.0 - sum_p2);
    }

    free(cnt);
    return 0;
}
