/*
 * 16-Point Complex FFT Implementation (Fixed-Point Version)
 * 
 * This implementation uses fixed-point arithmetic (Q15 format) to avoid
 * floating-point dependencies, making it suitable for embedded systems
 * without FPU or when soft-float libraries are not available.
 * 
 * Q15 format: 16-bit signed integers representing values in range [-1.0, 1.0)
 * Scale factor: 32768 (2^15)
 * 
 * Author: Generated for ZAP RISC-V Processor
 * Date: 2024
 */

// Disable division and modulo operations to avoid soft-math library dependencies
// Force single register operations to avoid LDM/STM instructions

#include "cfft.h"

// Compiler attributes to control instruction generation
#define AVOID_LDMSTM __attribute__((optimize("-fno-tree-loop-distribute-patterns")))

// Precomputed twiddle factors in Q15 format
// W_16^k = cos(2πk/16) - j*sin(2πk/16), scaled by 32768
static const complex_t twiddle_factors[8] = {
    {32767, 0},        // W_16^0 = 1.0 + 0.0j
    {30273, -12539},   // W_16^1 = 0.9239 - 0.3827j
    {23170, -23170},   // W_16^2 = 0.7071 - 0.7071j
    {12539, -30273},   // W_16^3 = 0.3827 - 0.9239j
    {0, -32767},       // W_16^4 = 0.0 - 1.0j
    {-12539, -30273},  // W_16^5 = -0.3827 - 0.9239j
    {-23170, -23170},  // W_16^6 = -0.7071 - 0.7071j
    {-30273, -12539}   // W_16^7 = -0.9239 - 0.3827j
};

// Bit-reversal lookup table for 16-point FFT
static const unsigned char bit_reverse_table[16] = {
    0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15
};

/**
 * Fixed-point complex multiplication with proper scaling
 * (a + bj) * (c + dj) = (ac - bd) + (ad + bc)j
 * Avoid struct return to prevent LDM/STM instructions
 */
static inline void complex_multiply(complex_t a, complex_t b, complex_t* result) {
    // Use 64-bit intermediate to avoid overflow, then scale back
    int64_t real_temp = ((int64_t)a.real * b.real - (int64_t)a.imag * b.imag);
    int64_t imag_temp = ((int64_t)a.real * b.imag + (int64_t)a.imag * b.real);
    
    // Scale back from Q30 to Q15
    result->real = (int32_t)(real_temp >> FIXED_POINT_BITS);
    result->imag = (int32_t)(imag_temp >> FIXED_POINT_BITS);
}

/**
 * Fixed-point complex addition - avoid struct return
 */
static inline void complex_add(complex_t a, complex_t b, complex_t* result) {
    result->real = a.real + b.real;
    result->imag = a.imag + b.imag;
}

/**
 * Fixed-point complex subtraction - avoid struct return
 */
static inline void complex_subtract(complex_t a, complex_t b, complex_t* result) {
    result->real = a.real - b.real;
    result->imag = a.imag - b.imag;
}

/**
 * Bit-reverse reordering for fixed-point data - avoid struct assignment
 */
static void bit_reverse_reorder(complex_t data[16]) {
    complex_t temp[16];
    
    // block9：Copy original data element by element to avoid LDM/STM
    for (int i = 0; i < 16; i++) {
        temp[i].real = data[i].real;
        temp[i].imag = data[i].imag;
    }
    
    // block11：Reorder according to bit-reverse table element by element
    for (int i = 0; i < 16; i++) {
        int src_idx = bit_reverse_table[i];
        data[i].real = temp[src_idx].real;
        data[i].imag = temp[src_idx].imag;
    }
}

/**
 * 16-Point Fixed-Point FFT using Cooley-Tukey algorithm
 * @param input: Array of 16 complex input samples in Q15 format
 * @param output: Array of 16 complex output samples in Q15 format
 */
// AVOID_LDMSTM
void fft_16_point(const complex_t input[16], complex_t output[16]) {
    // Copy input to output array for in-place computation - element by element
    // block7:
    for (int i = 0; i < 16; i++) {
        output[i].real = input[i].real;
        output[i].imag = input[i].imag;
    }
    
    // block8-11：Bit-reverse reordering
    bit_reverse_reorder(output);
    
    // FFT computation - 4 stages for 16-point FFT
    // Stage 1: m=2, step=8
    {
        //block13
        for (int group = 0; group < 16; group += 2) {
            int i = group;
            int j = group + 1;
            complex_t temp, u, v;
            
            // Multiply: temp = output[j] * twiddle_factors[0]
            complex_multiply(output[j], twiddle_factors[0], &temp);
            
            // Store operands
            u.real = output[i].real; u.imag = output[i].imag;
            v.real = temp.real; v.imag = temp.imag;
            
            // Butterfly operations
            complex_add(u, v, &output[i]);
            complex_subtract(u, v, &output[j]);
        }
    }
    
    // Stage 2: m=4, step=4
    {
        //block15
        for (int group = 0; group < 16; group += 4) {
            complex_t temp, u, v;
            
            // k=0
            {
                int i = group;
                int j = group + 2;
                
                complex_multiply(output[j], twiddle_factors[0], &temp);  // W_16^0
                u.real = output[i].real; u.imag = output[i].imag;
                v.real = temp.real; v.imag = temp.imag;
                complex_add(u, v, &output[i]);
                complex_subtract(u, v, &output[j]);
            }
            // k=1
            {
                int i = group + 1;
                int j = group + 3;
                
                complex_multiply(output[j], twiddle_factors[4], &temp);  // W_16^4
                u.real = output[i].real; u.imag = output[i].imag;
                v.real = temp.real; v.imag = temp.imag;
                complex_add(u, v, &output[i]);
                complex_subtract(u, v, &output[j]);
            }
        }
    }
    
    // Stage 3: m=8, step=2
    {
        //block17-19
        //#pragma clang loop unroll(disable)
        for (int group = 0; group < 16; group += 8) {
            complex_t temp, u, v;
            
            // k=0,1,2,3
            for (int k = 0; k < 4; k++) {
                int i = group + k;
                int j = group + k + 4;
                int twiddle_idx = (k << 1) & 7;  // k*2, masked to stay within bounds
                
                complex_multiply(output[j], twiddle_factors[twiddle_idx], &temp);
                u.real = output[i].real; u.imag = output[i].imag;
                v.real = temp.real; v.imag = temp.imag;
                complex_add(u, v, &output[i]);
                complex_subtract(u, v, &output[j]);
            }
        }
    }
    
    // Stage 4: m=16, step=1
    {
        complex_t temp, u, v;
        //block21
        for (int k = 0; k < 8; k++) {
            int i = k;
            int j = k + 8;
            
            complex_multiply(output[j], twiddle_factors[k], &temp);
            u.real = output[i].real; u.imag = output[i].imag;
            v.real = temp.real; v.imag = temp.imag;
            complex_add(u, v, &output[i]);
            complex_subtract(u, v, &output[j]);
        }
    }
}

/**
 * 16-Point Fixed-Point Inverse FFT
 * @param input: Array of 16 complex frequency domain samples in Q15 format
 * @param output: Array of 16 complex time domain samples in Q15 format
 */
// AVOID_LDMSTM
void ifft_16_point(const complex_t input[16], complex_t output[16]) {
    complex_t conjugated_input[16];
    
    // Conjugate the input - element by element
    for (int i = 0; i < 16; i++) {
        conjugated_input[i].real = input[i].real;
        conjugated_input[i].imag = -input[i].imag;
    }
    
    // Perform FFT on conjugated input
    fft_16_point(conjugated_input, output);
    
    // Conjugate and scale the output by 1/16 - element by element
    for (int i = 0; i < 16; i++) {
        output[i].real = output[i].real >> 4;  // Divide by 16
        output[i].imag = (-output[i].imag) >> 4;  // Conjugate and divide by 16
    }
}

/**
 * Simple magnitude calculation for fixed-point (without square root)
 * Returns magnitude squared for comparison purposes
 */
static uint32_t magnitude_squared(complex_t c) {
    int64_t real_sq = (int64_t)c.real * c.real;
    int64_t imag_sq = (int64_t)c.imag * c.imag;
    return (uint32_t)((real_sq + imag_sq) >> FIXED_POINT_BITS);
}

/**
 * Test 1: Fixed-point impulse signal test
 * Input: [1, 0, 0, ..., 0] in Q15 format
 * Expected: All frequency bins should have similar magnitude
 */
int test_impulse() {
    complex_t test_input[16];
    
    // Initialize manually to avoid memset/memcpy calls
    test_input[0].real = 32767; test_input[0].imag = 0;  // Impulse
    for (int i = 1; i < 16; i++) {
        test_input[i].real = 0;
        test_input[i].imag = 0;
    }
    
    complex_t fft_output[16];
    
    // ->BLOCK6-22
    fft_16_point(test_input, fft_output);
    
    // Verify results: all bins should have similar magnitude
    uint32_t reference_mag = magnitude_squared(fft_output[0]);
    uint32_t tolerance = reference_mag >> 3;  // 12.5% tolerance
    
    //block24-25
    for (int i = 1; i < 16; i++) {
        uint32_t mag = magnitude_squared(fft_output[i]);
        if (mag < (reference_mag - tolerance) || mag > (reference_mag + tolerance)) {
            return -1;  // Test failed
        }
    }
    
    return 0;  // Test passed
}

/**
 * Test 2: Fixed-point DC signal test
 * Input: [1, 1, 1, ..., 1] in Q15 format
 * Expected: Only DC component (bin 0) should be non-zero
 */
int test_dc() {
    complex_t test_input[16];
    
    // Initialize with DC value (all 1.0 in Q15 format)
    for (int i = 0; i < 16; i++) {
        test_input[i].real = 32767;
        test_input[i].imag = 0;
    }
    
    complex_t fft_output[16];
    
    // Perform fixed-point FFT
    fft_16_point(test_input, fft_output);
    
    // Check DC component (should be 16 * 32767)
    uint32_t dc_mag = magnitude_squared(fft_output[0]);
    uint32_t expected_dc = (uint32_t)16 * 32767;  // Approximate expected value
    
    // DC component should be much larger than others
    if (dc_mag < (expected_dc >> 2)) {  // At least 1/4 of expected
        return -1;
    }
    
    // Other components should be much smaller
    for (int i = 1; i < 16; i++) {
        uint32_t mag = magnitude_squared(fft_output[i]);
        if (mag > (dc_mag >> 4)) {  // Should be less than 1/16 of DC
            return -2;
        }
    }
    
    return 0;
}

/**
 * Test 3: Fixed-point single frequency test
 * Input: Precomputed cos(2πk/16) - j*sin(2πk/16) values in Q15 format
 * Expected: Energy concentrated at frequency bin 1
 */
int test_single_frequency() {
    complex_t test_input[16];
    
    // Precomputed values for frequency bin 1 in Q15 format - initialize manually
    test_input[0].real = 32767; test_input[0].imag = 0;        // k=0: 1.0 + 0.0j
    test_input[1].real = 30273; test_input[1].imag = -12539;   // k=1: 0.9239 - 0.3827j
    test_input[2].real = 23170; test_input[2].imag = -23170;   // k=2: 0.7071 - 0.7071j
    test_input[3].real = 12539; test_input[3].imag = -30273;   // k=3: 0.3827 - 0.9239j
    test_input[4].real = 0; test_input[4].imag = -32767;       // k=4: 0.0 - 1.0j
    test_input[5].real = -12539; test_input[5].imag = -30273;  // k=5: -0.3827 - 0.9239j
    test_input[6].real = -23170; test_input[6].imag = -23170;  // k=6: -0.7071 - 0.7071j
    test_input[7].real = -30273; test_input[7].imag = -12539;  // k=7: -0.9239 - 0.3827j
    test_input[8].real = -32767; test_input[8].imag = 0;       // k=8: -1.0 + 0.0j
    test_input[9].real = -30273; test_input[9].imag = 12539;   // k=9: -0.9239 + 0.3827j
    test_input[10].real = -23170; test_input[10].imag = 23170; // k=10: -0.7071 + 0.7071j
    test_input[11].real = -12539; test_input[11].imag = 30273; // k=11: -0.3827 + 0.9239j
    test_input[12].real = 0; test_input[12].imag = 32767;      // k=12: 0.0 + 1.0j
    test_input[13].real = 12539; test_input[13].imag = 30273;  // k=13: 0.3827 + 0.9239j
    test_input[14].real = 23170; test_input[14].imag = 23170;  // k=14: 0.7071 + 0.7071j
    test_input[15].real = 30273; test_input[15].imag = 12539;  // k=15: 0.9239 + 0.3827j
    
    complex_t fft_output[16];
    
    // Perform fixed-point FFT
    fft_16_point(test_input, fft_output);
    
    // Check that energy is concentrated at bin 1
    uint32_t bin1_mag = magnitude_squared(fft_output[1]);
    
    // All other bins should have much smaller magnitude
    for (int i = 0; i < 16; i++) {
        if (i == 1) continue;
        uint32_t mag = magnitude_squared(fft_output[i]);
        if (mag > (bin1_mag >> 4)) {  // Should be less than 1/16 of bin 1
            return -1;
        }
    }
    
    return 0;
}

/**
 * Test 4: IFFT correctness test
 * Verify that IFFT(FFT(x)) = x
 */
int test_ifft_correctness() {
    complex_t original[16];
    
    // Initialize impulse signal manually
    original[0].real = 32767; original[0].imag = 0;  // Impulse
    for (int i = 1; i < 16; i++) {
        original[i].real = 0;
        original[i].imag = 0;
    }
    
    complex_t fft_output[16];
    complex_t ifft_output[16];
    
    // Perform FFT then IFFT
    fft_16_point(original, fft_output);
    ifft_16_point(fft_output, ifft_output);
    
    // Check if we recovered the original signal
    int32_t tolerance = 1000;  // Allow some error due to fixed-point rounding
    
    for (int i = 0; i < 16; i++) {
        int32_t real_error = original[i].real - ifft_output[i].real;
        int32_t imag_error = original[i].imag - ifft_output[i].imag;
        
        if (real_error < 0) real_error = -real_error;
        if (imag_error < 0) imag_error = -imag_error;
        
        if (real_error > tolerance || imag_error > tolerance) {
            return -1;
        }
    }
    
    return 0;
}

/**
 * Comprehensive test function
 */
int test_fft_16() {
    int test_results[4];
    
    // Run all tests

    test_results[0] = test_impulse();
    test_results[1] = test_dc();
    test_results[2] = test_single_frequency();
    test_results[3] = test_ifft_correctness();
    
    // Check results
    int passed = 0;
    for (int i = 0; i < 4; i++) {
        if (test_results[i] == 0) {
            passed++;
        }
    }
    
    return (passed == 4) ? 0 : -1;
}

/**
 * Main function for testing
 */
int main() {
    // Test the FFT implementation
    int result = test_fft_16();
    
    // Return 0 for success, non-zero for failure
    return (result == 0) ? 0 : -1;
}