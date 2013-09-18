/*
 * Copyright (c) 2011-2013, The University of Oxford
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

#include <private_sky.h>

#include <oskar_mem.h>

#ifdef __cplusplus
extern "C" {
#endif

void oskar_sky_init(oskar_Sky* model, int type, int location,
        int num_elements, int* status)
{
    int owner = OSKAR_TRUE;

    /* Check all inputs. */
    if (!model || !status)
    {
        oskar_set_invalid_argument(status);
        return;
    }

    if (type != OSKAR_SINGLE && type != OSKAR_DOUBLE)
        *status = OSKAR_ERR_BAD_DATA_TYPE;

    if (location != OSKAR_LOCATION_CPU && location != OSKAR_LOCATION_GPU)
        *status = OSKAR_ERR_BAD_LOCATION;

    /* Set meta-data */
    model->precision = type;
    model->location = location;
    model->num_sources = num_elements;
    model->use_extended = OSKAR_FALSE;

    /* Initialise the memory. */
    oskar_mem_init(&model->RA, type, location, num_elements, owner, status);
    oskar_mem_init(&model->Dec, type, location, num_elements, owner, status);
    oskar_mem_init(&model->I, type, location, num_elements, owner, status);
    oskar_mem_init(&model->Q, type, location, num_elements, owner, status);
    oskar_mem_init(&model->U, type, location, num_elements, owner, status);
    oskar_mem_init(&model->V, type, location, num_elements, owner, status);
    oskar_mem_init(&model->reference_freq, type, location, num_elements, owner, status);
    oskar_mem_init(&model->spectral_index, type, location, num_elements, owner, status);
    oskar_mem_init(&model->l, type, location, num_elements, owner, status);
    oskar_mem_init(&model->m, type, location, num_elements, owner, status);
    oskar_mem_init(&model->n, type, location, num_elements, owner, status);
    oskar_mem_init(&model->FWHM_major, type, location, num_elements, owner, status);
    oskar_mem_init(&model->FWHM_minor, type, location, num_elements, owner, status);
    oskar_mem_init(&model->position_angle, type, location, num_elements, owner, status);
    oskar_mem_init(&model->gaussian_a, type, location, num_elements, owner, status);
    oskar_mem_init(&model->gaussian_b, type, location, num_elements, owner, status);
    oskar_mem_init(&model->gaussian_c, type, location, num_elements, owner, status);
}

#ifdef __cplusplus
}
#endif