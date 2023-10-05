/**
 *  @brief      Collection of Similarity Measures, SIMD-accelerated with SSE, AVX, NEON, SVE.
 *  @file       simsimd.h
 *  @author     Ash Vardanian
 *  @date       March 14, 2023
 *  @copyright  Copyright (c) 2023
 *
 *  References:
 *  x86 intrinsics: https://www.intel.com/content/www/us/en/docs/intrinsics-guide/
 *  Arm intrinsics: https://developer.arm.com/architectures/instruction-sets/intrinsics/
 *  Detecting target CPU features at compile time: https://stackoverflow.com/a/28939692/2766161
 */

#pragma once
#include "spatial.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    simsimd_metric_unknown_k = 0,

    // Classics:
    simsimd_metric_ip_k = 'i',
    simsimd_metric_dot_k = 'i',

    simsimd_metric_cos_k = 'c',
    simsimd_metric_angular_k = 'c',

    simsimd_metric_l2sq_k = 'e',
    simsimd_metric_euclidean_k = 'e',

    // Sets:
    simsimd_metric_hamming_k = 'b',
    simsimd_metric_tanimoto_k = 't',
} simsimd_metric_kind_t;

typedef enum {
    simsimd_cap_autovec_k = 0,

    simsimd_cap_arm_neon_k = 1 << 0,
    simsimd_cap_arm_sve_k = 1 << 1,
    simsimd_cap_arm_sve2_k = 1 << 2,

    simsimd_cap_x86_avx2_k = 1 << 10,
    simsimd_cap_x86_avx512_k = 1 << 11,
    simsimd_cap_x86_avx2fp16_k = 1 << 12,
    simsimd_cap_x86_avx512fp16_k = 1 << 13,

    simsimd_cap_x86_amx_k = 1 << 20,
    simsimd_cap_arm_sme_k = 1 << 21,

} simsimd_capability_t;

typedef enum {
    simsimd_datatype_unknown_k,
    simsimd_datatype_f64_k,
    simsimd_datatype_f32_k,
    simsimd_datatype_f16_k,
    simsimd_datatype_i8_k,
    simsimd_datatype_b1_k,
} simsimd_datatype_t;

typedef simsimd_f32_t (*simsimd_metric_punned_t)(void const*, void const*, simsimd_size_t, simsimd_size_t);

#if SIMSIMD_TARGET_X86

inline static unsigned _simsimd_capability_supported_x86(unsigned feature_mask, int function_id, int register_id) {
    unsigned eax, ebx, ecx, edx;
    // Execute CPUID instruction and store results in eax, ebx, ecx, edx
    // Setting %ecx to 0 as well
    __asm__ __volatile__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(function_id), "c"(0));
    unsigned register_value;
    switch (register_id) {
    case 0: register_value = eax; break;
    case 1: register_value = ebx; break;
    case 2: register_value = ecx; break;
    case 3: register_value = edx; break;
    default: return 0;
    }

    return (register_value & feature_mask) != 0;
}

#endif

inline static simsimd_capability_t simsimd_capabilities() {

#if SIMSIMD_TARGET_X86

    // Check for AVX2 (Function ID 7, EBX register)
    // https://github.com/llvm/llvm-project/blob/50598f0ff44f3a4e75706f8c53f3380fe7faa896/clang/lib/Headers/cpuid.h#L148
    unsigned supports_avx2 = _simsimd_capability_supported_x86(1 << 5, 7, 1);

    // Check for F16C (Function ID 1, ECX register)
    // https://github.com/llvm/llvm-project/blob/50598f0ff44f3a4e75706f8c53f3380fe7faa896/clang/lib/Headers/cpuid.h#L107
    unsigned supports_f16c = _simsimd_capability_supported_x86(1 << 29, 1, 2);

    // Check for AVX512F (Function ID 7, EBX register)
    // https://github.com/llvm/llvm-project/blob/50598f0ff44f3a4e75706f8c53f3380fe7faa896/clang/lib/Headers/cpuid.h#L155
    unsigned supports_avx512f = _simsimd_capability_supported_x86(1 << 16, 7, 1);

    // Check for AVX512FP16 (Function ID 7, EDX register)
    // https://github.com/llvm/llvm-project/blob/50598f0ff44f3a4e75706f8c53f3380fe7faa896/clang/lib/Headers/cpuid.h#L198C9-L198C23
    unsigned supports_avx512fp16 = _simsimd_capability_supported_x86(1 << 23, 7, 3);

    return (simsimd_capability_t)(                                                 //
        (simsimd_cap_x86_avx2_k * (supports_avx2)) |                               //
        (simsimd_cap_x86_avx512_k * (supports_avx512f)) |                          //
        (simsimd_cap_x86_avx2fp16_k * (supports_avx2 && supports_f16c)) |          //
        (simsimd_cap_x86_avx512fp16_k * (supports_avx512fp16 && supports_avx512f)) //
    );
#endif

#if SIMSIMD_TARGET_ARM

    // Every 64-bit Arm CPU supports NEON
    unsigned supports_neon = 1;
    unsigned long id_aa64isar0_el1 = 0;
    unsigned long id_aa64mmfr2_el1 = 0;

    // TODO: Fix capability checks on aarch64
    // https://github.com/Xilinx/linux-xlnx/blob/cb1f2dbc7e487361688257fccf78bf3f3ce0e9ed/arch/arm64/kernel/cpufeature.c#L2765
#if 0
    // Check the SVE and SVE2 field of ID_AA64ISAR0_EL1 register
    __asm__ __volatile__("mrs %0, id_aa64isar0_el1" : "=r"(id_aa64isar0_el1));
    // Check the MML field of ID_AA64MMFR2_EL1 register
    __asm__ __volatile__("mrs %0, id_aa64mmfr2_el1" : "=r"(id_aa64mmfr2_el1));
#endif

    unsigned supports_sve = (id_aa64isar0_el1 & 0x00000000000000f0) != 0;
    unsigned supports_sve2 = (id_aa64isar0_el1 & 0x000000000000f000) != 0;
    unsigned supports_sme = (id_aa64mmfr2_el1 & 0x00000000000000f0) != 0;

    return (simsimd_capability_t)(                 //
        (simsimd_cap_arm_neon_k * supports_neon) | //
        (simsimd_cap_arm_sve_k * supports_sve) |   //
        (simsimd_cap_arm_sve2_k * supports_sve2) | //
        (simsimd_cap_arm_sme_k * supports_sme)     //
    );

#endif

    return simsimd_cap_autovec_k;
}

/**
 *  @brief  Depending on the hardware capabilities of the CPU detected at runtime, the list of pre-compiled metrics,
 *          and list of preferences from the `allowed`, determines the best suited distance function
 *          implementation.
 */
inline static simsimd_metric_punned_t simsimd_metric_punned( //
    simsimd_metric_kind_t kind,                              //
    simsimd_datatype_t datatype,                             //
    simsimd_capability_t allowed) {

    simsimd_capability_t supported = simsimd_capabilities();
    simsimd_capability_t viable = (simsimd_capability_t)(supported & allowed);

    switch (datatype) {

        // Single-precision floating-point vectors
    case simsimd_datatype_f32_k:

#if SIMSIMD_TARGET_ARM_NEON
        if (viable & simsimd_cap_arm_neon_k)
            switch (kind) {
            case simsimd_metric_ip_k: return (simsimd_metric_punned_t)&simsimd_neon_f32_ip;
            case simsimd_metric_cos_k: return (simsimd_metric_punned_t)&simsimd_neon_f32_cos;
            case simsimd_metric_l2sq_k: return (simsimd_metric_punned_t)&simsimd_neon_f32_l2sq;
            }
#endif
        switch (kind) {
        case simsimd_metric_ip_k: return (simsimd_metric_punned_t)&simsimd_auto_f32_ip;
        case simsimd_metric_cos_k: return (simsimd_metric_punned_t)&simsimd_auto_f32_cos;
        case simsimd_metric_l2sq_k: return (simsimd_metric_punned_t)&simsimd_auto_f32_l2sq;
        }

        // Half-precision floating-point vectors
    case simsimd_datatype_f16_k:

#if SIMSIMD_TARGET_ARM_NEON
        if (viable & simsimd_cap_arm_neon_k)
            switch (kind) {
            case simsimd_metric_ip_k: return (simsimd_metric_punned_t)&simsimd_neon_f16_ip;
            case simsimd_metric_cos_k: return (simsimd_metric_punned_t)&simsimd_neon_f16_cos;
            case simsimd_metric_l2sq_k: return (simsimd_metric_punned_t)&simsimd_neon_f16_l2sq;
            }
#endif
#if SIMSIMD_TARGET_ARM_SVE
        if (viable & simsimd_cap_arm_sve_k)
            switch (kind) {
            case simsimd_metric_ip_k: return (simsimd_metric_punned_t)&simsimd_sve_f16_ip;
            case simsimd_metric_cos_k: return (simsimd_metric_punned_t)&simsimd_sve_f16_cos;
            case simsimd_metric_l2sq_k: return (simsimd_metric_punned_t)&simsimd_sve_f16_l2sq;
            }
#endif
#if SIMSIMD_TARGET_X86_AVX2
        if (viable & simsimd_cap_x86_avx2_k)
            switch (kind) {
            case simsimd_metric_ip_k: return (simsimd_metric_punned_t)&simsimd_avx2_f16_ip;
            case simsimd_metric_cos_k: return (simsimd_metric_punned_t)&simsimd_avx2_f16_cos;
            case simsimd_metric_l2sq_k: return (simsimd_metric_punned_t)&simsimd_avx2_f16_l2sq;
            }
#endif
#if SIMSIMD_TARGET_X86_AVX512
        if (viable & simsimd_cap_x86_avx512_k)
            switch (kind) {
            case simsimd_metric_ip_k: return (simsimd_metric_punned_t)&simsimd_avx512_f16_ip;
            case simsimd_metric_cos_k: return (simsimd_metric_punned_t)&simsimd_avx512_f16_cos;
            case simsimd_metric_l2sq_k: return (simsimd_metric_punned_t)&simsimd_avx512_f16_l2sq;
            }
#endif

        switch (kind) {
        case simsimd_metric_ip_k: return (simsimd_metric_punned_t)&simsimd_auto_f16_ip;
        case simsimd_metric_cos_k: return (simsimd_metric_punned_t)&simsimd_auto_f16_cos;
        case simsimd_metric_l2sq_k: return (simsimd_metric_punned_t)&simsimd_auto_f16_l2sq;
        }

    // Single-byte integer vectors
    case simsimd_datatype_i8_k:
#if SIMSIMD_TARGET_ARM_NEON
        if (viable & simsimd_cap_arm_neon_k)
            switch (kind) {
            case simsimd_metric_ip_k: return (simsimd_metric_punned_t)&simsimd_neon_i8_ip;
            case simsimd_metric_cos_k: return (simsimd_metric_punned_t)&simsimd_neon_i8_cos;
            case simsimd_metric_l2sq_k: return (simsimd_metric_punned_t)&simsimd_neon_i8_l2sq;
            }
#endif
#if SIMSIMD_TARGET_X86_AVX2
        if (viable & simsimd_cap_x86_avx2_k)
            switch (kind) {
            case simsimd_metric_ip_k: return (simsimd_metric_punned_t)&simsimd_avx2_i8_ip;
            case simsimd_metric_cos_k: return (simsimd_metric_punned_t)&simsimd_avx2_i8_cos;
            case simsimd_metric_l2sq_k: return (simsimd_metric_punned_t)&simsimd_avx2_i8_l2sq;
            }
#endif

        switch (kind) {
        case simsimd_metric_ip_k: return (simsimd_metric_punned_t)&simsimd_auto_i8_ip;
        case simsimd_metric_cos_k: return (simsimd_metric_punned_t)&simsimd_auto_i8_cos;
        case simsimd_metric_l2sq_k: return (simsimd_metric_punned_t)&simsimd_auto_i8_l2sq;
        }
    }
}

#ifdef __cplusplus
}
#endif
