/*
 * Copyright (c) 2013, The University of Oxford
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

#include "oskar_convert_lon_lat_to_tangent_plane_direction.h"

#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif


/* Single precision. */
void oskar_convert_lon_lat_to_tangent_plane_direction_f(int num_positions,
        float lon0, float lat0, const float* lon, const float* lat, float* x,
        float* y)
{
    int i;
    float sinLat0, cosLat0;
    sinLat0 = sin(lat0);
    cosLat0 = cos(lat0);

    for (i = 0; i < num_positions; ++i)
    {
        float cosLat, sinLat, sinLon, cosLon, relLon, pLat, x_, y_;
        pLat = lat[i];
        relLon = lon[i];
        relLon -= lon0;
        sinLon = sinf(relLon);
        cosLon = cosf(relLon);
        sinLat = sinf(pLat);
        cosLat = cosf(pLat);
        x_ = cosLat * sinLon;
        y_ = cosLat0 * sinLat - sinLat0 * cosLat * cosLon;
        x[i] = x_;
        y[i] = y_;
    }
}

/* Double precision. */
void oskar_convert_lon_lat_to_tangent_plane_direction_d(int num_positions,
        double lon0, double lat0, const double* lon, const double* lat,
        double* x, double* y)
{
    int i;
    double sinLat0, cosLat0;
    sinLat0 = sin(lat0);
    cosLat0 = cos(lat0);

    for (i = 0; i < num_positions; ++i)
    {
        double cosLat, sinLat, sinLon, cosLon, relLon, pLat, x_, y_;
        pLat = lat[i];
        relLon = lon[i];
        relLon -= lon0;
        sinLon = sin(relLon);
        cosLon = cos(relLon);
        sinLat = sin(pLat);
        cosLat = cos(pLat);
        x_ = cosLat * sinLon;
        y_ = cosLat0 * sinLat - sinLat0 * cosLat * cosLon;
        x[i] = x_;
        y[i] = y_;
    }
}

/* Single precision OpenMP. */
void oskar_convert_lon_lat_to_tangent_plane_direction_omp_f(int num_positions,
        float lon0, float lat0, const float* lon, const float* lat, float* x,
        float* y)
{
    int i;
    float sinLat0, cosLat0;
    sinLat0 = sin(lat0);
    cosLat0 = cos(lat0);

    #pragma omp parallel for
    for (i = 0; i < num_positions; ++i)
    {
        float cosLat, sinLat, sinLon, cosLon, relLon, pLat, x_, y_;
        pLat = lat[i];
        relLon = lon[i];
        relLon -= lon0;
        sinLon = sinf(relLon);
        cosLon = cosf(relLon);
        sinLat = sinf(pLat);
        cosLat = cosf(pLat);
        x_ = cosLat * sinLon;
        y_ = cosLat0 * sinLat - sinLat0 * cosLat * cosLon;
        x[i] = x_;
        y[i] = y_;
    }
}

/* Double precision OpenMP. */
void oskar_convert_lon_lat_to_tangent_plane_direction_omp_d(int num_positions,
        double lon0, double lat0, const double* lon, const double* lat,
        double* x, double* y)
{
    int i;
    double sinLat0, cosLat0;
    sinLat0 = sin(lat0);
    cosLat0 = cos(lat0);

    #pragma omp parallel for
    for (i = 0; i < num_positions; ++i)
    {
        double cosLat, sinLat, sinLon, cosLon, relLon, pLat, x_, y_;
        pLat = lat[i];
        relLon = lon[i];
        relLon -= lon0;
        sinLon = sin(relLon);
        cosLon = cos(relLon);
        sinLat = sin(pLat);
        cosLat = cos(pLat);
        x_ = cosLat * sinLon;
        y_ = cosLat0 * sinLat - sinLat0 * cosLat * cosLon;
        x[i] = x_;
        y[i] = y_;
    }
}


#ifdef __cplusplus
}
#endif