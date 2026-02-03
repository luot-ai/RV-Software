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

// 扭结因子表（W_N^k, k=0..N/2-1），用递推生成，避免三角函数
static complex_t twiddle_factors[FFT_N / 2];
static int twiddles_initialized = 0;
// 生成 W_N^k 表：w_{k+1} = w_k * W1，W1 ≈ cos(2π/N) - j sin(2π/N)
static void init_twiddles(void) {
    if (twiddles_initialized) return;

    // Q15 近似：cos(2π/1024)≈0.999981, sin≈0.0061359
    const complex_t W1 = { 32767, -(int16_t)50 }; // cos=32767, -sin=-201

    complex_t w = { 32767, 0 }; // W^0 = 1
    for (int k = 0; k < FFT_N / 2; k++) {
        twiddle_factors[k] = w;
        complex_t next;
        complex_multiply(w, W1, &next);
        w = next;
    }
    twiddles_initialized = 1;
}

#define N2 32   // 外层长度
#define N1 128   // 内层长度
#define FFT_32 32
#define LOG2_FFT_32 5
#define FFT_128 128
#define LOG2_FFT_128 7

static inline uint16_t bit_reverse_u16(uint16_t x, int bits) {
    uint16_t r = 0;
    for (int i = 0; i < bits; i++) {
        r = (uint16_t)((r << 1) | (x & 1));
        x >>= 1;
    }
    return r;
}

static void bit_reverse_32(complex_t* data) {
    for (uint16_t i = 0; i < FFT_32; i++) {
        uint16_t j = bit_reverse_u16(i, LOG2_FFT_32);
        if (j > i) {
            complex_t t = data[i];
            data[i] = data[j];
            data[j] = t;
        }
    }
}

//52 54 56 58 60  【54到58被展开了】
void fft_32_point(complex_t output[FFT_32]) {
    init_twiddles();
    bit_reverse_32(output); //blk48-50

    // 分段蝶形：m=2,4,...,1024；step=N/m，从 N/2 开始每级右移一位
    int m = 2;//蝶形长度
    int step = FFT_N >> 1; // N/m，旋转因子步长
    while (m <= FFT_32) {
        int halfsize = m >> 1;//也是每段的stride，即每段内相隔多少个元素进行蝶形运算

        for (int block = 0; block < FFT_32; block += m) {
            int k = 0;
            for (int j = 0; j < halfsize; j++) {
                complex_t t, u, v;
                complex_multiply(output[block + j + halfsize], twiddle_factors[k], &t);

                u = output[block + j];
                v = t;

                complex_add(u, v, &output[block + j]);
                complex_subtract(u, v, &output[block + j + halfsize]);

                k += step; // k = j * (N/m)
            }
        }
        //下一级，蝶形长度翻倍，旋转因子步长减半
        m <<= 1;
        step >>= 1;
    }
}


static void bit_reverse_128(complex_t* data) {
    for (unsigned short i = 0; i < FFT_128; i++) {
        unsigned short j = bit_reverse_u16(i, LOG2_FFT_128);
        if (j > i) {
            complex_t t = data[i];
            data[i] = data[j];
            data[j] = t;
        }
    }
}

void fft_128_point(complex_t output[FFT_128]) {
    init_twiddles();
    bit_reverse_128(output);

    // 分段蝶形：m=2,4,...,1024；step=N/m，从 N/2 开始每级右移一位
    int m = 2;//蝶形长度
    int step = FFT_N >> 1; // N/m，旋转因子步长
    while (m <= FFT_128) {
        int halfsize = m >> 1;//也是每段的stride，即每段内相隔多少个元素进行蝶形运算

        for (int block = 0; block < FFT_128; block += m) {
            int k = 0;
            for (int j = 0; j < halfsize; j++) {
                complex_t t, u, v;
                complex_multiply(output[block + j + halfsize], twiddle_factors[k], &t);

                u = output[block + j];
                v = t;

                complex_add(u, v, &output[block + j]);
                complex_subtract(u, v, &output[block + j + halfsize]);

                k += step; // k = j * (N/m)
            }
        }
        //下一级，蝶形长度翻倍，旋转因子步长减半
        m <<= 1;
        step >>= 1;
    }
}

// void fft_128_stockham(const complex_t input[FFT_128], complex_t output[FFT_128])
// {
//     init_twiddles();
//     complex_t y[FFT_128];
//     for (int i = 0; i < FFT_128; i++) {
//         output[i] = input[i];
//     }

//     complex_t *src = output;
//     complex_t *dst = y;

//     int s = 1;          /* stride */

//     for (int stage = 0; stage < LOG2_FFT_128; stage++) {
//         int m = FFT_128 >> (stage + 1);      /* current half size */

//         for (int p = 0; p < m; p++) {
//             int tw_idx = (p << stage) << 3;
//             complex_t wp = twiddle_factors[tw_idx];

//             for (int q = 0; q < s; q++) {
//                 complex_t a = src[q + s * (p + 0)];
//                 complex_t b = src[q + s * (p + m)];

//                 complex_add(a, b, &dst[q + s * (2 * p + 0)]);
//                 complex_t t;
//                 complex_subtract(a,b,&t);
//                 complex_multiply(t, wp, &dst[q + s * (2 * p + 1)]);
//             }
//         }

//         /* 下一层 */
//         s <<= 1;

//         /* ping-pong buffer */
//         complex_t *tmp = src;
//         src = dst;
//         dst = tmp;
//     }

//     /* 如果最终结果在 y，拷回 x */
//     if (src != output) {
//         for (int i = 0; i < FFT_128; i++)
//             output[i] = src[i];
//     }

// }


#define BLOCK_M 16
#define BLOCK_N 8
void transpose_mxn_block_16x8(
    const complex_t *restrict src,
    complex_t *restrict dst,
    int M,   // rows
    int N    // cols
) {
    for (int ib = 0; ib < M; ib += BLOCK_M) {
        int imax = (ib + BLOCK_M <= M) ? ib + BLOCK_M : M;

        for (int jb = 0; jb < N; jb += BLOCK_N) {
            int jmax = (jb + BLOCK_N <= N) ? jb + BLOCK_N : N;

            /* transpose one 16x8 (or edge) block */
            for (int i = ib; i < imax; i++) {
                const complex_t *src_row = src + i * N + jb;
                for (int j = jb; j < jmax; j++) {
                    dst[j * M + i] = src_row[j - jb];
                }
            }
        }
    }
}

static complex_t temp[N1][N2];
void fft_4K_block(const complex_t input[FFT_N], complex_t output[FFT_N]) {
    init_twiddles();

    // Stage 1: N2 × N1 FFT     1.258s
    for (int b = 0; b < N2; b++) {
        for (int i = 0; i < N1; i++) {
            output[b * N1 + i] =
                input[b * N1 + i];
        }
        fft_128_point(&output[b * N1]);
    }

    // Stage 2: block twiddle  263.8ms
    for (int b = 1; b < N2; b++) {
        for (int k = 0; k < N1; k++) {
            int tw = (b * k) & (FFT_N - 1);
            complex_t w;
            if (tw < FFT_N / 2) {
                w = twiddle_factors[tw];
            } else {
                w = twiddle_factors[tw - FFT_N / 2];
                w.real = -w.real;
                w.imag = -w.imag;
            }
            complex_multiply(output[b * N1 + k],w,&output[b * N1 + k]);
        }
    }

    // Stage 3: transpose  82.78ms
    transpose_mxn_block_16x8(&output[0], &temp[0][0], 128, 32);

    // Stage 4: N1 × N2 FFT  672.1ms
    for (int b = 0; b < N1; b++) {
        fft_32_point(temp[b]);
    }
    // Stage 5: transpose  90.83ms
    transpose_mxn_block_16x8(&temp[0][0], &output[0], 32, 128);

}

static complex_t test_input[FFT_N];
static complex_t fft_output[FFT_N];

int main() {
    for (int i = 0; i < FFT_N; i++) {
        test_input[i].real = 32767;
        test_input[i].imag = 0;
    }    
    fft_4K_block(test_input, fft_output);
    return 0;
}
