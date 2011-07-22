/*
 * Copyright (c) 2011, The University of Oxford
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the University of Oxford nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/cuda/oskar_modules_cuda_correlator_lm.h"

#include "cuda/kernels/oskar_cudak_rpw3leglm.h"
#include "cuda/kernels/oskar_cudak_mat_mul_cc.h"
#include "math/oskar_math_mat_tri_c.h"
#include "interferometry/oskar_synthesis_baselines.h"
#include "interferometry/oskar_synthesis_xyz2uvw.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cublas.h>

#include "cuda/CudaEclipse.h"

#ifdef __cplusplus
extern "C" {
#endif

// Single precision.

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

int oskar_modules_cudaf_correlator_lm(int na, const float* ax, const float* ay,
        const float* az, int ns, const float* l, const float* m,
        const float* bsqrt, const float* e, float ra0, float dec0,
        float lst0, int nsdt, float sdt, float k, float* vis, float* u,
        float* v, float* w)
{
    // Initialise.
    cudaError_t errCuda = cudaSuccess;
    cublasStatus errCublas = CUBLAS_STATUS_SUCCESS;
    double lst, ha0; // Must be double.
    float2 one = make_float2(1.0f, 0.0f);
    int i, a, retVal = 0;
    double tIncCentre, tInc;
    double tOffset = (double)sdt / 2.0;
    cublasInit();

    // Set up thread blocks.
    dim3 kThd(64, 4); // Sources, antennas.
    dim3 kBlk((ns + kThd.x - 1) / kThd.x, (na + kThd.y - 1) / kThd.y);
    size_t sMem = (kThd.x + kThd.y) * sizeof(float3);
    dim3 mThd(64, 4); // Sources, antennas.
    dim3 mBlk((ns + mThd.x - 1) / mThd.x, (na + mThd.y - 1) / mThd.y);
    dim3 vThd(16, 16); // Antennas, antennas.
    dim3 vBlk((na + vThd.x - 1) / vThd.x, (na + vThd.y - 1) / vThd.y);

    // Compute the source n-coordinates from l and m.
    float* n = (float*)malloc(ns * sizeof(float));
    float r = 0.0f;
    for (i = 0; i < ns; ++i)
    {
        r = l[i]*l[i] + m[i]*m[i];
        n[i] = (r < 1.0) ? sqrtf(1.0f - r) - 1.0f : -1.0f;
    }

    // Scale source brightnesses (in Bsqrt) by station beams (in E).
    float2* eb = (float2*)malloc(ns * na * sizeof(float2));
    for (a = 0; a < na; ++a)
    {
        for (i = 0; i < ns; ++i)
        {
            int idx = i + a * ns;
            float bs = bsqrt[i];
            eb[idx].x = e[2*idx + 0] * bs; // Real
            eb[idx].y = e[2*idx + 1] * bs; // Imag
        }
    }

    // Allocate host memory for station u,v,w coordinates and visibility matrix.
    float* uvw = (float*)malloc(na * 3 * sizeof(float));
    float* vism = (float*)malloc(na * na * sizeof(float2));

    // Allocate memory for source coordinates and visibility matrix on the
    // device.
    float *ld, *md, *nd, *uvwd;
    float2 *visd, *kmat, *emat;
    cudaMalloc((void**)&ld, ns * sizeof(float));
    cudaMalloc((void**)&md, ns * sizeof(float));
    cudaMalloc((void**)&nd, ns * sizeof(float));
    cudaMalloc((void**)&visd, na * na * sizeof(float2));
    cudaMalloc((void**)&kmat, ns * na * sizeof(float2));
    cudaMalloc((void**)&emat, ns * na * sizeof(float2));
    cudaMalloc((void**)&uvwd, 3 * na * sizeof(float));
    cudaThreadSynchronize();
    errCuda = cudaPeekAtLastError();
    if (errCuda != cudaSuccess) goto stop;

    // Copy source coordinates and modified E-matrix to device.
    cudaMemcpy(ld, l, ns * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(md, m, ns * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(nd, n, ns * sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(emat, eb, ns * na * sizeof(float2), cudaMemcpyHostToDevice);

    // Clear visibility matrix.
    cudaMemset(visd, 0, na * na * sizeof(float2));

    // Loop over integrations.
    for (i = 0; i < nsdt; ++i)
    {
        // Compute the current LST and hour angle of the phase centre.
        tInc = i * sdt + tOffset;
        lst = lst0 + 2 * M_PI * tInc / 86400.0;
        ha0 = lst - ra0;

        // Compute the station u,v,w coordinates.
        oskar_synthesisf_xyz2uvw(na, ax, ay, az, ha0, dec0,
                &uvw[0], &uvw[na], &uvw[2*na]);

        // Copy u,v,w coordinates to device.
        cudaMemcpy(uvwd, uvw, na * 3 * sizeof(float), cudaMemcpyHostToDevice);

        // Compute K-matrix.
        oskar_cudakf_rpw3leglm <<<kBlk, kThd, sMem>>> (
                na, uvwd, ns, ld, md, nd, k, kmat);
        cudaThreadSynchronize();
        errCuda = cudaPeekAtLastError();
        if (errCuda != cudaSuccess) goto stop;

        // Perform complex matrix element multiply.
        oskar_cudakf_mat_mul_cc <<<mBlk, mThd>>> (ns, na, kmat, emat, kmat);
        cudaThreadSynchronize();
        errCuda = cudaPeekAtLastError();
        if (errCuda != cudaSuccess) goto stop;

        // Perform matrix-matrix multiply-accumulate (reduction).
        cublasCgemm('c', 'n', na, na, ns, one, kmat, ns,
                kmat, ns, one, visd, na);
        errCublas = cublasGetError();
        if (errCublas != CUBLAS_STATUS_SUCCESS) goto stop;
    }

    // Scale result.
    cublasCscal(na * na, make_float2(1.0f / nsdt, 0.0f), visd, 1);
    errCublas = cublasGetError();
    if (errCublas != CUBLAS_STATUS_SUCCESS) goto stop;

    // Copy result to host.
    cudaMemcpy(vism, visd, na * na * sizeof(float2), cudaMemcpyDeviceToHost);

    // Extract triangular half.
    oskar_mathf_mat_tri_c(na, vism, vis);

    // Copy u,v,w baseline coordinates of mid-point to output arrays.
    // FIXME: probably don't need to return UVW from this function?
    tIncCentre = ((nsdt - 1) / 2) * sdt + tOffset;
    lst = lst0 + 2 * M_PI * tIncCentre * sdt / 86400.0f;
    ha0 = lst - ra0;
    oskar_synthesisf_xyz2uvw(na, ax, ay, az, ha0, dec0,
            &uvw[0], &uvw[na], &uvw[2*na]);
    oskar_synthesisf_baselines(na, &uvw[0], &uvw[na], &uvw[2*na],
            u, v, w);

    // Clean up before exit.
    stop:
    if (errCuda != cudaSuccess)
    {
        retVal = errCuda;
        printf("CUDA Error: %s\n", cudaGetErrorString(errCuda));
    }
    if (errCublas != CUBLAS_STATUS_SUCCESS)
    {
        retVal = errCublas;
        printf("CUBLAS Error: Code %d\n", errCublas);
    }

    // Free host memory.
    free(vism);
    free(uvw);
    free(eb);
    free(n);

    // Free device memory.
    cudaFree(kmat);
    cudaFree(emat);
    cudaFree(uvwd);
    cudaFree(ld);
    cudaFree(md);
    cudaFree(nd);
    cudaFree(visd);

    // Shutdown.
    cublasShutdown();
    return retVal;
}

// Double precision.

#ifndef M_PI_D
#define M_PI_D 3.14159265358979323846
#endif

int oskar_modules_cudad_correlator_lm(int na, const double* ax, const double* ay,
        const double* az, int ns, const double* l, const double* m,
        const double* bsqrt, const double* e, double ra0, double dec0,
        double lst0, int nsdt, double sdt, double k, double* vis, double* u,
        double* v, double* w)
{
    // Initialise.
    cudaError_t errCuda = cudaSuccess;
    cublasStatus errCublas = CUBLAS_STATUS_SUCCESS;
    double lst, ha0;
    double2 one = make_double2(1.0, 0.0);
    int i, a, retVal = 0;
    double tIncCentre, tInc;
    double tOffset = (double)sdt / 2.0;
    cublasInit();

    // Set up thread blocks.
    dim3 kThd(64, 4); // Sources, antennas.
    dim3 kBlk((ns + kThd.x - 1) / kThd.x, (na + kThd.y - 1) / kThd.y);
    size_t sMem = (kThd.x + kThd.y) * sizeof(double3);
    dim3 mThd(64, 4); // Sources, antennas.
    dim3 mBlk((ns + mThd.x - 1) / mThd.x, (na + mThd.y - 1) / mThd.y);
    dim3 vThd(16, 16); // Antennas, antennas.
    dim3 vBlk((na + vThd.x - 1) / vThd.x, (na + vThd.y - 1) / vThd.y);

    // Compute the source n-coordinates from l and m.
    double* n = (double*)malloc(ns * sizeof(double));
    double r = 0.0;
    for (i = 0; i < ns; ++i)
    {
        r = l[i]*l[i] + m[i]*m[i];
        n[i] = (r < 1.0) ? sqrt(1.0 - r) - 1.0 : -1.0;
    }

    // Scale source brightnesses (in Bsqrt) by station beams (in E).
    double2* eb = (double2*)malloc(ns * na * sizeof(double2));
    for (a = 0; a < na; ++a)
    {
        for (i = 0; i < ns; ++i)
        {
            int idx = i + a * ns;
            double bs = bsqrt[i];
            eb[idx].x = e[2*idx + 0] * bs; // Real
            eb[idx].y = e[2*idx + 1] * bs; // Imag
        }
    }

    // Allocate host memory for station u,v,w coordinates and visibility matrix.
    double* uvw = (double*)malloc(na * 3 * sizeof(double));
    double* vism = (double*)malloc(na * na * sizeof(double2));

    // Allocate memory for source coordinates and visibility matrix on the
    // device.
    double *ld, *md, *nd, *uvwd;
    double2 *visd, *kmat, *emat;
    cudaMalloc((void**)&ld, ns * sizeof(double));
    cudaMalloc((void**)&md, ns * sizeof(double));
    cudaMalloc((void**)&nd, ns * sizeof(double));
    cudaMalloc((void**)&visd, na * na * sizeof(double2));
    cudaMalloc((void**)&kmat, ns * na * sizeof(double2));
    cudaMalloc((void**)&emat, ns * na * sizeof(double2));
    cudaMalloc((void**)&uvwd, 3 * na * sizeof(double));
    cudaThreadSynchronize();
    errCuda = cudaPeekAtLastError();
    if (errCuda != cudaSuccess) goto stop;

    // Copy source coordinates and modified E-matrix to device.
    cudaMemcpy(ld, l, ns * sizeof(double), cudaMemcpyHostToDevice);
    cudaMemcpy(md, m, ns * sizeof(double), cudaMemcpyHostToDevice);
    cudaMemcpy(nd, n, ns * sizeof(double), cudaMemcpyHostToDevice);
    cudaMemcpy(emat, eb, ns * na * sizeof(double2), cudaMemcpyHostToDevice);

    // Clear visibility matrix.
    cudaMemset(visd, 0, na * na * sizeof(double2));

    // Loop over integrations.
    for (i = 0; i < nsdt; ++i)
    {
        // Compute the current LST and hour angle of the phase centre.
        tInc = i * sdt + tOffset;
        lst = lst0 + 2 * M_PI * tInc / 86400.0;
        ha0 = lst - ra0;

        // Compute the station u,v,w coordinates.
        oskar_synthesisd_xyz2uvw(na, ax, ay, az, ha0, dec0,
                &uvw[0], &uvw[na], &uvw[2*na]);

        // Copy u,v,w coordinates to device.
        cudaMemcpy(uvwd, uvw, na * 3 * sizeof(double), cudaMemcpyHostToDevice);

        // Compute K-matrix.
        oskar_cudakd_rpw3leglm <<<kBlk, kThd, sMem>>> (
                na, uvwd, ns, ld, md, nd, k, kmat);
        cudaThreadSynchronize();
        errCuda = cudaPeekAtLastError();
        if (errCuda != cudaSuccess) goto stop;

        // Perform complex matrix element multiply.
        oskar_cudakd_mat_mul_cc <<<mBlk, mThd>>> (ns, na, kmat, emat, kmat);
        cudaThreadSynchronize();
        errCuda = cudaPeekAtLastError();
        if (errCuda != cudaSuccess) goto stop;

        // Perform matrix-matrix multiply-accumulate (reduction).
        cublasZgemm('c', 'n', na, na, ns, one, kmat, ns,
                kmat, ns, one, visd, na);
        errCublas = cublasGetError();
        if (errCublas != CUBLAS_STATUS_SUCCESS) goto stop;
    }

    // Scale result.
    cublasZscal(na * na, make_double2(1.0 / nsdt, 0.0), visd, 1);
    errCublas = cublasGetError();
    if (errCublas != CUBLAS_STATUS_SUCCESS) goto stop;

    // Copy result to host.
    cudaMemcpy(vism, visd, na * na * sizeof(double2), cudaMemcpyDeviceToHost);

    // Extract triangular half.
    oskar_mathd_mat_tri_c(na, vism, vis);

    // Copy u,v,w baseline coordinates of mid-point to output arrays.
    // FIXME: probably don't need to return UVW from this function?
    tIncCentre = ((nsdt - 1) / 2) * sdt + tOffset;
    lst = lst0 + 2 * M_PI * tIncCentre * sdt / 86400.0f;
    ha0 = lst - ra0;
    oskar_synthesisd_xyz2uvw(na, ax, ay, az, ha0, dec0,
            &uvw[0], &uvw[na], &uvw[2*na]);
    oskar_synthesisd_baselines(na, &uvw[0], &uvw[na], &uvw[2*na],
            u, v, w);

    // Clean up before exit.
    stop:
    if (errCuda != cudaSuccess)
    {
        retVal = errCuda;
        printf("CUDA Error: %s\n", cudaGetErrorString(errCuda));
    }
    if (errCublas != CUBLAS_STATUS_SUCCESS)
    {
        retVal = errCublas;
        printf("CUBLAS Error: Code %d\n", errCublas);
    }

    // Free host memory.
    free(vism);
    free(uvw);
    free(eb);
    free(n);

    // Free device memory.
    cudaFree(kmat);
    cudaFree(emat);
    cudaFree(uvwd);
    cudaFree(ld);
    cudaFree(md);
    cudaFree(nd);
    cudaFree(visd);

    // Shutdown.
    cublasShutdown();
    return retVal;
}

#ifdef __cplusplus
}
#endif
