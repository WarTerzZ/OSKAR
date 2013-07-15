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

#ifndef OSKAR_SKY_MODEL_H_
#define OSKAR_SKY_MODEL_H_

/**
 * @file oskar_SkyModel.h
 */

#include "oskar_global.h"
#include "utility/oskar_Mem.h"
#include <stdlib.h>

/**
 * @struct oskar_SkyModel
 *
 * @brief Structure to hold a sky model.
 *
 * @details
 * The structure holds source parameters for a sky model.
 */
struct OSKAR_EXPORT oskar_SkyModel
{
    int num_sources;          /**< Number of sources in the sky model. */
    oskar_Mem RA;             /**< Right ascension, in radians. */
    oskar_Mem Dec;            /**< Declination, in radians. */
    oskar_Mem I;              /**< Stokes-I, in Jy. */
    oskar_Mem Q;              /**< Stokes-Q, in Jy. */
    oskar_Mem U;              /**< Stokes-U, in Jy. */
    oskar_Mem V;              /**< Stokes-V, in Jy. */
    oskar_Mem reference_freq; /**< Reference frequency for the source flux, in Hz. */
    oskar_Mem spectral_index; /**< Spectral index. */
    oskar_Mem l;              /**< Phase centre relative l-direction cosines. */
    oskar_Mem m;              /**< Phase centre relative m-direction cosines. */
    oskar_Mem n;              /**< Phase centre relative n-direction cosines. */

    int use_extended;         /**< Enable use of extended sources */
    oskar_Mem FWHM_major;     /**< Major axis FWHM for gaussian sources, in radians. */
    oskar_Mem FWHM_minor;     /**< Minor axis FWHM for gaussian sources, in radians. */
    oskar_Mem position_angle; /**< Position angle for gaussian sources, in radians. */
    oskar_Mem gaussian_a;     /**< Gaussian source width parameter */
    oskar_Mem gaussian_b;     /**< Gaussian source width parameter */
    oskar_Mem gaussian_c;     /**< Gaussian source width parameter */

#ifdef __cplusplus
    /**
     * @brief Constructs and allocates memory for a sky model.
     *
     * @param type         Data type for the sky model (either OSKAR_SINGLE,
     *                     or OSKAR_DOUBLE)
     * @param location     Memory location of the sky model (either
     *                     OSKAR_LOCATION_CPU or OSKAR_LOCATION_GPU)
     * @param num_sources  Number of sources in the sky model.
     */
    oskar_SkyModel(int type = OSKAR_DOUBLE, int location = OSKAR_LOCATION_CPU,
            int num_sources = 0);

    /**
     * @brief Constructs a sky model by copying the supplied sky
     * model to the specified location.
     *
     * @param sky          oskar_SkyModel to copy.
     * @param location     Memory location of the constructed sky model
     *                     (either OSKAR_LOCATION_CPU or OSKAR_LOCATION_GPU)
     */
    oskar_SkyModel(const oskar_SkyModel* other, int location);

    /**
     * @brief Destroys the sky model.
     */
    ~oskar_SkyModel();

    /**
     * @brief Returns the memory location for memory in the sky structure
     * or error code if the types are inconsistent.
     */
    int location() const;

    /**
     * @brief Returns the memory type for memory in the sky structure
     * or error code if the types are inconsistent.
     */
    int type() const;
#endif
};
typedef struct oskar_SkyModel oskar_SkyModel;

/* To maintain binary compatibility, do not change the values
 * in the lists below. */
enum {
    OSKAR_SKY_TAG_NUM_SOURCES = 1,
    OSKAR_SKY_TAG_DATA_TYPE = 2,
    OSKAR_SKY_TAG_RA = 3,
    OSKAR_SKY_TAG_DEC = 4,
    OSKAR_SKY_TAG_STOKES_I = 5,
    OSKAR_SKY_TAG_STOKES_Q = 6,
    OSKAR_SKY_TAG_STOKES_U = 7,
    OSKAR_SKY_TAG_STOKES_V = 8,
    OSKAR_SKY_TAG_REF_FREQ = 9,
    OSKAR_SKY_TAG_SPECTRAL_INDEX = 10,
    OSKAR_SKY_TAG_FWHM_MAJOR = 11,
    OSKAR_SKY_TAG_FWHM_MINOR = 12,
    OSKAR_SKY_TAG_POSITION_ANGLE = 13
};

#endif /* OSKAR_SKY_MODEL_H_ */
