/*
 * Copyright (c) 2011-2020, The University of Oxford
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

#include "interferometer/private_interferometer.h"
#include "interferometer/oskar_interferometer.h"
#include "vis/oskar_vis_block_write_ms.h"
#include "vis/oskar_vis_header_write_ms.h"

#ifdef __cplusplus
extern "C" {
#endif

void oskar_interferometer_write_block(oskar_Interferometer* h,
        const oskar_VisBlock* block, int block_index, int* status)
{
    if (*status) return;
    oskar_timer_resume(h->tmr_write);
#ifndef OSKAR_NO_MS
    if (h->ms_name && !h->ms)
        h->ms = oskar_vis_header_write_ms(h->header, h->ms_name, OSKAR_TRUE,
                h->force_polarised_ms, status);
    if (h->ms) oskar_vis_block_write_ms(block, h->header, h->ms, status);
#endif
    if (h->vis_name && !h->vis)
        h->vis = oskar_vis_header_write(h->header, h->vis_name, status);
    if (h->vis) oskar_vis_block_write(block, h->vis, block_index, status);
    oskar_timer_pause(h->tmr_write);
}

#ifdef __cplusplus
}
#endif
