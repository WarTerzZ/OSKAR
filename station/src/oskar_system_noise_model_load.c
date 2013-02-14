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

#include "station/oskar_system_noise_model_load.h"
#include "utility/oskar_getline.h"
#include "utility/oskar_mem_free.h"
#include "utility/oskar_mem_init.h"
#include "utility/oskar_mem_realloc.h"
#include "utility/oskar_vector_types.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

void oskar_system_noise_model_load(oskar_Mem* mem, const char* filename,
        int* status)
{
    int n = 0;
    char* line = NULL;
    FILE* file;
    size_t bufsize = 0;

    /* Check all inputs. */
    if (!mem || !filename || !status)
    {
        oskar_set_invalid_argument(status);
        return;
    }

    /* Check if safe to proceed. */
    if (*status) return;

    /* Check location */
    if (mem->location != OSKAR_LOCATION_CPU)
    {
        *status = OSKAR_ERR_BAD_LOCATION;
        return;
    }

    /* Check the data type. */
    if (!(mem->type == OSKAR_SINGLE || mem->type == OSKAR_DOUBLE))
    {
        *status = OSKAR_ERR_BAD_DATA_TYPE;
        return;
    }

    /* Open the file. */
    file = fopen(filename, "r");
    if (!file)
    {
        *status = OSKAR_ERR_FILE_IO;
        return;
    }

    /* Read the file. */
    while (oskar_getline(&line, &bufsize, file) != OSKAR_ERR_EOF)
    {
        int a;
        double value = 0.0;

        /* Ignore comment lines (lines starting with '#'). */
        if (line[0] == '#') continue;

        /* Parse the line. */
        a = sscanf(line, "%lf\n", &value);

        /* Check that the data was read correctly. */
        if (a != 1) continue;

        /* Ensure enough space in the array. */
        if (mem->num_elements <= n)
        {
            oskar_mem_realloc(mem, n + 1, status);
            if (*status) goto cleanup;
        }

        /* Store the value. */
        if (mem->type == OSKAR_SINGLE)
            ((float*)mem->data)[n] = (float)value;
        else if (mem->type == OSKAR_DOUBLE)
            ((double*)mem->data)[n] = value;

        /* Increment the array pointer. */
        ++n;
    }

    /* Record the number of elements loaded. */
    mem->num_elements = n;

    /* Free the line buffer and close the file. */
    cleanup:
    if (line) free(line);
    fclose(file);
}

#ifdef __cplusplus
}
#endif