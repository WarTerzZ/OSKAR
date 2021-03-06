/*
 * Copyright (c) 2013-2019, The University of Oxford
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

#include "telescope/private_telescope.h"
#include "telescope/oskar_telescope.h"

#include <stdlib.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void oskar_telescope_duplicate_first_station(oskar_Telescope* telescope,
        int* status)
{
    int i = 0;

    /* Copy the first station to the others. */
    for (i = 1; i < telescope->num_stations; ++i)
    {
        /* Preserve the coordinates. */
        const double lon = oskar_station_lon_rad(telescope->station[i]);
        const double lat = oskar_station_lat_rad(telescope->station[i]);
        const double alt = oskar_station_alt_metres(telescope->station[i]);
        const double x = oskar_station_offset_ecef_x(telescope->station[i]);
        const double y = oskar_station_offset_ecef_y(telescope->station[i]);
        const double z = oskar_station_offset_ecef_z(telescope->station[i]);

        /* Copy the first station if not duplicating the beam,
         * otherwise just free all the others. */
        oskar_station_free(telescope->station[i], status);
        telescope->station[i] = 0;
        if (!oskar_telescope_allow_station_beam_duplication(telescope))
        {
            telescope->station[i] = oskar_station_create_copy(
                    telescope->station[0],
                    telescope->mem_location, status);

            /* Reinstate the coordinates. */
            oskar_station_set_position(telescope->station[i],
                    lon, lat, alt, x, y, z);
        }
    }
}

#ifdef __cplusplus
}
#endif
