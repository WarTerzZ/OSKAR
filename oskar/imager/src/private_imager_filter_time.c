/*
 * Copyright (c) 2017-2019, The University of Oxford
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

#include "imager/private_imager.h"

#include "imager/private_imager_filter_time.h"
#include <float.h>

#ifdef __cplusplus
extern "C" {
#endif

void oskar_imager_filter_time(oskar_Imager* h, size_t* num_vis,
        oskar_Mem* uu, oskar_Mem* vv, oskar_Mem* ww, oskar_Mem* amp,
        oskar_Mem* weight, oskar_Mem* time_centroid, int* status)
{
    size_t i;
    double t, range[2], *time_centroid_;

    /* Return immediately if filtering is not enabled. */
    if ((h->time_min_utc <= 0.0 && h->time_max_utc <= 0.0) ||
            !time_centroid ||
            oskar_mem_length(time_centroid) == 0)
        return;
    if (*status) return;

    /* Get the range. */
    range[0] = h->time_min_utc;
    range[1] = (h->time_max_utc <= 0.0) ? (double) FLT_MAX : h->time_max_utc;

    /* Get the number of input points, and set the number selected to zero. */
    const size_t n = *num_vis;
    *num_vis = 0;

    /* Apply the time centroid filter. */
    oskar_timer_resume(h->tmr_filter);
    time_centroid_ = oskar_mem_double(time_centroid, status);
    if (h->imager_prec == OSKAR_DOUBLE)
    {
        double2* amp_ = 0;
        double *uu_, *vv_, *ww_, *weight_;
        uu_ = oskar_mem_double(uu, status);
        vv_ = oskar_mem_double(vv, status);
        ww_ = oskar_mem_double(ww, status);
        weight_ = oskar_mem_double(weight, status);
        if (!h->coords_only)
            amp_ = oskar_mem_double2(amp, status);

        for (i = 0; i < n; ++i)
        {
            t = time_centroid_[i];
            if (t >= range[0] && t <= range[1])
            {
                uu_[*num_vis] = uu_[i];
                vv_[*num_vis] = vv_[i];
                ww_[*num_vis] = ww_[i];
                weight_[*num_vis] = weight_[i];
                time_centroid_[*num_vis] = t;
                if (amp_) amp_[*num_vis] = amp_[i];
                (*num_vis)++;
            }
        }
    }
    else
    {
        float2* amp_ = 0;
        float *uu_, *vv_, *ww_, *weight_;
        uu_ = oskar_mem_float(uu, status);
        vv_ = oskar_mem_float(vv, status);
        ww_ = oskar_mem_float(ww, status);
        weight_ = oskar_mem_float(weight, status);
        if (!h->coords_only)
            amp_ = oskar_mem_float2(amp, status);

        for (i = 0; i < n; ++i)
        {
            t = time_centroid_[i];
            if (t >= range[0] && t <= range[1])
            {
                uu_[*num_vis] = uu_[i];
                vv_[*num_vis] = vv_[i];
                ww_[*num_vis] = ww_[i];
                weight_[*num_vis] = weight_[i];
                time_centroid_[*num_vis] = t;
                if (amp_) amp_[*num_vis] = amp_[i];
                (*num_vis)++;
            }
        }
    }
    oskar_timer_pause(h->tmr_filter);
}

#ifdef __cplusplus
}
#endif
