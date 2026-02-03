// ...existing code...
#include "cfft.h"
// #include <stdio.h>
#include <stdint.h>
#define FFT_N 4096
#define LOG2_FFT_N 12


//复数运算
static inline void complex_multiply(complex_t a, complex_t b, complex_t* result) {
    // Use 64-bit intermediate to avoid overflow, then scale back
    int64_t real_temp = ((int64_t)a.real * b.real - (int64_t)a.imag * b.imag);
    int64_t imag_temp = ((int64_t)a.real * b.imag + (int64_t)a.imag * b.real);
    
    // Scale back from Q30 to Q15
    result->real = (int32_t)(real_temp >> FIXED_POINT_BITS);
    result->imag = (int32_t)(imag_temp >> FIXED_POINT_BITS);
}
static inline void complex_add(complex_t a, complex_t b, complex_t* result) {
    result->real = a.real + b.real;
    result->imag = a.imag + b.imag;
}
static inline void complex_subtract(complex_t a, complex_t b, complex_t* result) {
    result->real = a.real - b.real;
    result->imag = a.imag - b.imag;
}



// 16位数据的位反转 
// 13：72239：1.84
// static inline uint16_t bit_reverse_u16(uint16_t x, int bits) {
//     uint16_t r = 0;
//     for (int i = 0; i < bits; i++) {
//         r = (uint16_t)((r << 1) | (x & 1));
//         x >>= 1;
//     }
//     return r;
// }
//
//复杂度：O(N * LOG2_FFT_N)，通过逐次计算 rev(i) 避免预生成查表。
// 位反转重排（就地）
// 15：56280 0.6
// static void bit_reverse_reorder_generic(complex_t* data) {
//     for (uint16_t i = 0; i < FFT_N; i++) {
//         uint16_t j = bit_reverse_u16(i, LOG2_FFT_N);
//         if (j > i) { //14：17327：0.89
//             complex_t t = data[i];
//             data[i] = data[j];
//             data[j] = t;
//         }
//     }
// }

// 扭结因子表（W_N^k, k=0..N/2-1），用递推生成，避免三角函数
static complex_t twiddle_factors[FFT_N / 2];
static int twiddles_initialized = 0;
// 生成 W_N^k 表：w_{k+1} = w_k * W1，W1 ≈ cos(2π/N) - j sin(2π/N)
static void init_twiddles(void) {
    if (twiddles_initialized) return;

    // Q15 近似：cos(2π/1024)≈0.999981, sin≈0.0061359
    const complex_t W1 = { 32767, -(int16_t)201 }; // cos=32767, -sin=-201

    complex_t w = { 32767, 0 }; // W^0 = 1
    for (int k = 0; k < FFT_N / 2; k++) {
        twiddle_factors[k] = w;
        complex_t next;
        complex_multiply(w, W1, &next);
        w = next;
    }
    twiddles_initialized = 1;
}

void fft_4096_stockham(const complex_t input[FFT_N], complex_t output[4096])
{
    init_twiddles();
    complex_t y[4096];
    complex_t *src = output;
    complex_t *dst = y;

    int s = 1;          /* stride */
    for (int i = 0; i < FFT_N; i++) {
        output[i] = input[i];
    }
    for (int stage = 0; stage < 12; stage++) {
        int m = 4096 >> (stage + 1);      /* current half size */

        for (int p = 0; p < m; p++) {
            int tw_idx = p << stage;
            complex_t wp = twiddle_factors[tw_idx];

            for (int q = 0; q < s; q++) {
                complex_t a = src[q + s * (p + 0)];
                complex_t b = src[q + s * (p + m)];

                complex_add(a, b, &dst[q + s * (2 * p + 0)]);
                complex_t t;
                complex_subtract(a,b,&t);
                complex_multiply(t, wp, &dst[q + s * (2 * p + 1)]);
            }
        }

        /* 下一层 */
        s <<= 1;

        /* ping-pong buffer */
        complex_t *tmp = src;
        src = dst;
        dst = tmp;
    }

    /* 如果最终结果在 y，拷回 x */
    if (src != output) {
        for (int i = 0; i < 4096; i++)
            output[i] = src[i];
    }

}



static complex_t test_input[FFT_N];
static complex_t fft_output[FFT_N];

int main(){
    for (int i = 0; i < FFT_N; i++) {
        test_input[i].real = 32767;
        test_input[i].imag = 0;
    }    

    fft_4096_stockham(test_input,fft_output);
    return 0;
}



