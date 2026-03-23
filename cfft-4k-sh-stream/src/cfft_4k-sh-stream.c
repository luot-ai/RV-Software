// ...existing code...
#include "cfft.h"
// #include <stdio.h>
#include <stdint.h>
#define FFT_N 4096
#define LOG2_FFT_N 12

//000
static int cfg_i(int outerIter,int length,int fifo_id)
{
    int rs1 = (outerIter & 0xffff) | ((length & 0xffff) << 16);
    asm volatile (
       ".insn r 0x0b, 0, 0, x0, %0, %1"
             :
             :"r"(rs1),"r"(fifo_id)
     );
    return 0; 
}

static int cfg_i_limit(int limit,int fifo_id)
{
    asm volatile (
       ".insn r 0x0b, 0, 1, x0, %0, %1"
             :
             :"r"(limit),"r"(fifo_id)
     );
    return 0; 
}

static int cfg_i_repeat(int repeat,int fifo_id)
{
    asm volatile (
       ".insn r 0x0b, 0, 2, x0, %0, %1"
             :
             :"r"(repeat),"r"(fifo_id)
     );
    return 0; 
}


//001
// static int cfg_store(uint32_t addr,int fifo_id)
// {
//     asm volatile (
//        ".insn r 0x0b, 1, 0, x0, %0, %1"
//              :
//              :"r"(addr),"r"(fifo_id)
//      );
//     return 0; 
// }

//010 
// TODO 目前是硬件直接确定src为0和1，dst为2，后续可以改成指令编码控制
// static int cal_stream(int fifo_id_src0,int fifo_id_src1,int fifo_id_dst)
// {
// 	int rs1 = (fifo_id_src0 & 0x3) | ((fifo_id_src1 & 0x3) << 2);
//     asm volatile (
//        ".insn r 0x0b, 2, 0, x0, %0, %1"
//              :
//              :"r"(rs1),"r"(fifo_id_dst)
//      );
//     return 0; 
// }

//  TODO
//  流流 -> 流，dst不是固定为2，而是0和1的合体
//  为这个指令增加一系列配置指令，满足索引需求
//  base=0，limit = 2, stride = limit << 1, LENGTH32 change LINE, DOUBLE
static int cal_stream_pp(int fifo_id_src0,int fifo_id_src1,int fifo_id_dst)
{
	int rs1 = (fifo_id_src0 & 0x3) | ((fifo_id_src1 & 0x3) << 2);
    asm volatile (
       ".insn r 0x0b, 2, 1, x0, %0, %1"
             :
             :"r"(rs1),"r"(fifo_id_dst)
     );
    return 0; 
}


//011
static int cfg_stride(uint32_t stride,int fifo_id)
{
    asm volatile (
       ".insn r 0x0b, 3, 0, x0, %0, %1"
             :
             :"r"(stride),"r"(fifo_id)
     );
    return 0; 
}

static int cfg_tilestride(uint32_t tilestride,int fifo_id)
{
    asm volatile (
       ".insn r 0x0b, 3, 1, x0, %0, %1"
             :
             :"r"(tilestride),"r"(fifo_id)
     );
    return 0; 
}

//100
static int cfg_reuse(uint32_t reuse,int fifo_id)
{
    asm volatile (
       ".insn r 0x0b, 4, 0, x0, %0, %1"
             :
             :"r"(reuse),"r"(fifo_id)
     );
    return 0; 
}

//101
static int cfg_load(uint32_t addr,int fifo_id)
{
    asm volatile (
       ".insn r 0x0b, 5, 0, x0, %0, %1"
             :
             :"r"(addr),"r"(fifo_id)
     );
    return 0; 
}

// static int cfg_axi_load(uint32_t addr,int fifo_id)
// {
//     asm volatile (
//        ".insn r 0x0b, 5, 1, x0, %0, %1"
//              :
//              :"r"(addr),"r"(fifo_id)
//      );
//     return 0; 
// }


// 110: 寄寄流指令
// TODO
// dst流限定为 12号流的合体
// base=0+limit，limit = 2, stride = limit << 1, LENGTH32 change LINE, DOUBLE
static int cal_rjrk_stream_pp(uint32_t lo, uint32_t hi)
{
    asm volatile (
       ".insn r 0x0b, 6, 0, x0, %0, %1"
       :
       : "r"(lo), "r"(hi)
     );
    return 0; 
}

//111
// static int cal_stream_rd(int fifo_id_src0,int fifo_id_src1)
// {
//     int rd;
// 	int rs1 = (fifo_id_src0 & 0x3) | ((fifo_id_src1 & 0x3) << 2);
//     asm volatile (
//        ".insn r 0x0b, 7, 0, %0, %1, x0"
//              :"=r"(rd) 
//              :"r"(rs1)
//      );
//     return rd; 
// }

static int cal_stream_rd_sub(int fifo_id_src0,int fifo_id_src1)
{
    int rd;
	int rs1 = (fifo_id_src0 & 0x3) | ((fifo_id_src1 & 0x3) << 2);
    asm volatile (
       ".insn r 0x0b, 7, 8, %0, %1, x0"
             :"=r"(rd) 
             :"r"(rs1)
     );
    return rd; 
}



static inline void store_shifted(int64_t x)
{
    int32_t hi = (int32_t)(x >> 32);
    int32_t lo = (int32_t)(x);

    asm volatile (
        "slli %0, %0, 17\n\t"
        "srli %1, %1, 15\n\t"
        : "+r"(hi), "+r"(lo)   
    );

    cal_rjrk_stream_pp(lo, hi);
}


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


static inline void complex_multiply_stream(complex_t a, complex_t b) {
    // Use 64-bit intermediate to avoid overflow, then scale back
    int64_t real_temp = ((int64_t)a.real * b.real - (int64_t)a.imag * b.imag);
    int64_t imag_temp = ((int64_t)a.real * b.imag + (int64_t)a.imag * b.real);
    
    // Scale back from Q30 to Q15
    store_shifted(real_temp);
    store_shifted(imag_temp);
}


//TODO dst固定为0和1的合体
//流流 -> 流 
//该函数可以替代汇编里的 add sw add sw(其实也省了lw)
static inline void complex_add_stream( ) {
    cal_stream_pp(0,1,0); //src流索引+1,reuse-1 = 1
    cal_stream_pp(0,1,0); //src流索引+1,->limit repeat
}

// TODO 通过funct7增加 减法
// 流流 寄
static inline void complex_subtract_stream(complex_t* result) {
    cal_stream_rd_sub(0,1);//src流索引+1,reuse-1 = 0
    cal_stream_rd_sub(0,1);//src流索引+1,->limit ,no repeat
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

static complex_t twiddle_stage_128[FFT_128/2];
static complex_t twiddle_stage_32[FFT_32/2];

static void reshape_twiddles(void)
{
    init_twiddles();
    for (int p = 0; p < FFT_128/2; p++) {
        twiddle_stage_128[p] = twiddle_factors[ p << 5 ];
    }
    for (int p = 0; p < FFT_32/2; p++) {
        twiddle_stage_32[p] = twiddle_stage_128[ p << 2 ];
    }
}


void fft_128_stockham(complex_t output[FFT_128])
{
    complex_t y[FFT_128];
    complex_t *src = output;
    complex_t *dst = y;

    int s = 1;          /* stride */

    for (int stage = 0; stage < LOG2_FFT_128; stage++) {
        int m = FFT_128 >> (stage + 1);      /* current half size */

        for (int p = 0; p < m; p++) {
            int tw_idx = (p << stage) ;
            complex_t wp = twiddle_stage_128[tw_idx];

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
        for (int i = 0; i < FFT_128; i++)
            output[i] = src[i];
    }

}


void fft_32_stockham(complex_t output[FFT_32])
{
    complex_t y[FFT_32];
    complex_t *src = output;
    complex_t *dst = y;

    int s = 1;          /* stride */

    for (int stage = 0; stage < LOG2_FFT_32; stage++) {
        int m = FFT_32 >> (stage + 1);      /* current half size */

        for (int p = 0; p < m; p++) {
            int tw_idx = (p << stage);
            complex_t wp = twiddle_stage_32[tw_idx];

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
        for (int i = 0; i < FFT_32; i++)
            output[i] = src[i];
    }

}

void fft_32_stockham_stream(complex_t output[FFT_32])
{
    complex_t y[FFT_32];
    complex_t *src = output;
    complex_t *dst = y;

    int s = 1;          /* stride */

    for (int stage = 0; stage < LOG2_FFT_32 - 1; stage++) {
        int m = FFT_32 >> (stage + 1);      /* current half size */

        for (int p = 0; p < m; p++) {
            int tw_idx = (p << stage);
            complex_t wp = twiddle_stage_32[tw_idx];

            for (int q = 0; q < s; q++) {
                complex_add_stream();
                complex_t t;
                complex_subtract_stream(&t);
                complex_multiply_stream(t, wp);
            }
        }

        /* 下一层 */
        s <<= 1;

        /* ping-pong buffer */
        complex_t *tmp = src;
        src = dst;
        dst = tmp;
    }

    // last-stage
    int stage = LOG2_FFT_32;
    int m = FFT_32 >> (stage + 1);
    for (int p = 0; p < m; p++) {
        int tw_idx = (p << stage);
        complex_t wp = twiddle_stage_32[tw_idx];

        for (int q = 0; q < s; q++) {
            complex_add_stream();
            complex_t t;
            complex_subtract_stream(&t);
            complex_multiply(t, wp, &dst[q + s * (2 * p + 1)]); //write to areg，not stream buffer 
        }
    }

    complex_t *tmp = src;
    src = dst;
    dst = tmp;

    /* 如果最终结果在 y，拷回 x */
    if (src != output) {
        for (int i = 0; i < FFT_32; i++)
            output[i] = src[i];
    }

}

#define BLOCK_M 8
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
    reshape_twiddles();
    // Stage 1: N2 × N1 FFT
    for (int b = 0; b < N2; b++) {
        for (int i = 0; i < N1; i++) {
            output[b * N1 + i] =
                input[b * N1 + i];
        }
        fft_128_stockham(&output[b * N1]);
    }

    // Stage 2: block twiddle
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

    // Stage 3: transpose
    transpose_mxn_block_16x8(&output[0], &temp[0][0], 32, 128);

    // Stage 4: N1 × N2 FFT

    for (int b = 0; b < N1; b++) 
    {
        fft_32_stockham(temp[b]);
    }

    // Stage 5: transpose
    transpose_mxn_block_16x8(&temp[0][0], &output[0], 128, 32);

}

// int main() {
//     static complex_t test_input[FFT_N];
//     static complex_t fft_output[FFT_N];
//     for (int i = 0; i < FFT_N; i++) {
//         test_input[i].real = 32767;
//         test_input[i].imag = 0;
//     }    
//     fft_4K_block(test_input, fft_output);
//     return 0;
// }

//just for test
int main() {
    //static complex_t test_input[FFT_N];
    //static complex_t fft_output[FFT_N];
    for (int i = 0; i < FFT_N; i++) {
        temp[i/32][i%32].real = 32767;
        temp[i/32][i%32].imag = 0;
    }    

    int dontCare = 4096;
    int l2LineByte = 128;
	cfg_i(1,32,0);  
	cfg_i(1,32,1); 
    cfg_i_limit(dontCare,0); 
    cfg_i_limit(dontCare,1);
    cfg_i_repeat(1,0);  
    cfg_i_repeat(1,1); 
    cfg_reuse(2,0);  //reuse for complex add/sub
    cfg_reuse(2,1);
    cfg_stride(4,0); 
    cfg_stride(4,1); 
    cfg_tilestride(l2LineByte,0); 
    cfg_tilestride(l2LineByte,1); 
    cfg_load((uint32_t)temp,0); //这条指令必须在 配置stride指令之后
	cfg_load((uint32_t)&temp[0][16],1);

    for (int b = 0; b < N1; b++) 
    {
        fft_32_stockham_stream(temp[b]);
    }
    return 0;
}