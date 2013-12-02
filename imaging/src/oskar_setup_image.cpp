/*
 * Copyright (c) 2012-2013, The University of Oxford
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


#include "imaging/oskar_setup_image.h"
#include "imaging/oskar_image_resize.h"
#include "imaging/oskar_image_evaluate_ranges.h"

#include <cstdio>

#define SEC2DAYS 1.15740740740740740740741e-5

#ifdef __cplusplus
extern "C" {
#endif

int oskar_setup_image(oskar_Image* im, const oskar_Vis* vis,
        const oskar_SettingsImage* settings)
{
    int err = OSKAR_SUCCESS;

    // Polarisation settings.
    int im_type = settings->image_type;
    int num_pols = 0;
    if (im_type == OSKAR_IMAGE_TYPE_STOKES_I ||
            im_type == OSKAR_IMAGE_TYPE_STOKES_Q ||
            im_type == OSKAR_IMAGE_TYPE_STOKES_U ||
            im_type == OSKAR_IMAGE_TYPE_STOKES_V ||
            im_type == OSKAR_IMAGE_TYPE_POL_XX ||
            im_type == OSKAR_IMAGE_TYPE_POL_YY ||
            im_type == OSKAR_IMAGE_TYPE_POL_XY ||
            im_type == OSKAR_IMAGE_TYPE_POL_YX ||
            im_type == OSKAR_IMAGE_TYPE_PSF)
    {
        num_pols = 1;
    }
    else if (im_type == OSKAR_IMAGE_TYPE_STOKES ||
            im_type == OSKAR_IMAGE_TYPE_POL_LINEAR)
    {
        num_pols = 4;
    }
    else
    {
        return OSKAR_ERR_BAD_DATA_TYPE;
    }

    // Set the channel range for the image cube [output range].
    int im_chan_range[2];
    err = oskar_evaluate_image_range(im_chan_range, settings->channel_snapshots,
            settings->channel_range, oskar_vis_num_channels(vis));
    if (err) return err;
    int im_num_chan = im_chan_range[1] - im_chan_range[0] + 1;

    // Set the time range for the image cube [output range].
    int im_time_range[2];
    err = oskar_evaluate_image_range(im_time_range, settings->time_snapshots,
            settings->time_range, oskar_vis_num_times(vis));
    if (err) return err;
    int im_num_times = im_time_range[1] - im_time_range[0] + 1;

    // Time and channel range for data.
    int vis_chan_range[2];
    err = oskar_evaluate_image_data_range(vis_chan_range, settings->channel_range,
            oskar_vis_num_channels(vis));
    if (err) return err;
    int vis_time_range[2];
    err = oskar_evaluate_image_data_range(vis_time_range, settings->time_range,
            oskar_vis_num_times(vis));
    if (err) return err;

    // Resize the image cube
    oskar_image_resize(im, settings->size, settings->size,
            num_pols, im_num_times, im_num_chan, &err);
    if (err) return err;

    // Set image meta-data
    // __Note__ the dimension order used here is assumed unchanged from that
    // defined in oskar_image_init()
    oskar_mem_init(&im->settings_path, OSKAR_CHAR, OSKAR_LOCATION_CPU,
            (int)oskar_mem_length(oskar_vis_settings_path_const(vis)), 1, &err);
    oskar_mem_copy(&im->settings_path, oskar_vis_settings_path_const(vis), &err);
    if (err) return err;

    im->fov_ra_deg = settings->fov_deg;
    im->fov_dec_deg = settings->fov_deg;

    if (settings->direction_type == OSKAR_IMAGE_DIRECTION_OBSERVATION)
    {
        im->centre_ra_deg = oskar_vis_phase_centre_ra_deg(vis);
        im->centre_dec_deg = oskar_vis_phase_centre_dec_deg(vis);
    }
    else if (settings->direction_type == OSKAR_IMAGE_DIRECTION_RA_DEC)
    {
        im->centre_ra_deg = settings->ra_deg;
        im->centre_dec_deg = settings->dec_deg;
    }
    else
        return OSKAR_ERR_SETTINGS_IMAGE;

    im->time_start_mjd_utc = oskar_vis_time_start_mjd_utc(vis) +
            (vis_time_range[0] * oskar_vis_time_inc_seconds(vis) * SEC2DAYS);
    // TODO for time synthesis the time inc should be 0...? need to determine
    // difference between inc and integration time.
    im->time_inc_sec = (settings->time_snapshots) ?
            oskar_vis_time_inc_seconds(vis) : 0.0;

    // TODO for channel synthesis the time inc should be 0...? need to determine
    // difference between inc and channel bandwidth.
    im->freq_inc_hz = (settings->channel_snapshots) ?
            oskar_vis_freq_inc_hz(vis) : 0.0;
    if (settings->channel_snapshots)
    {
        im->freq_start_hz = oskar_vis_freq_start_hz(vis) +
                vis_chan_range[0] * oskar_vis_freq_inc_hz(vis);
    }
    else
    {
        double chan0 = (vis_chan_range[1] - vis_chan_range[0]) / 2.0;
        im->freq_start_hz = oskar_vis_freq_start_hz(vis) +
                chan0 * oskar_vis_freq_inc_hz(vis);
    }
    im->image_type         = settings->image_type;
    im->coord_frame        = OSKAR_IMAGE_COORD_FRAME_EQUATORIAL;
    im->grid_type          = OSKAR_IMAGE_GRID_TYPE_RECTILINEAR;

    return OSKAR_SUCCESS;
}

#ifdef __cplusplus
}
#endif
