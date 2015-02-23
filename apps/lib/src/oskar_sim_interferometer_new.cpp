/*
 * Copyright (c) 2011-2015, The University of Oxford
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

#include <cuda_runtime_api.h>

#include <oskar_sim_interferometer_new.h>

#include <oskar_convert_ecef_to_station_uvw.h>
#include <oskar_convert_ecef_to_baseline_uvw.h>
#include <oskar_convert_mjd_to_gast_fast.h>
#include <oskar_correlate.h>
#include <oskar_cuda_mem_log.h>
#include <oskar_evaluate_jones_R.h>
#include <oskar_evaluate_jones_Z.h>
#include <oskar_evaluate_jones_E.h>
#include <oskar_evaluate_jones_K.h>
#include <oskar_jones.h>
#include <oskar_log.h>
#include <oskar_round_robin.h>
#include <oskar_set_up_sky.h>
#include <oskar_set_up_telescope.h>
#include <oskar_set_up_vis.h>
#include <oskar_settings_free.h>
#include <oskar_settings_load.h>
#include <oskar_settings_log.h>
#include <oskar_sky.h>
#include <oskar_station_work.h>
#include <oskar_telescope.h>
#include <oskar_timer.h>
#include <oskar_vis_block.h>
#include <oskar_vis_block_write_ms.h>
#include <oskar_vis_header.h>
#include <oskar_vis_header_write_ms.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#include <cstdlib>
#include <cmath>
#include <vector>
#include <string>
#include <sstream>
#include <cstdarg>

// Memory allocated per GPU.
struct DeviceData
{
    // Host memory.
    oskar_VisBlock* vis_block_cpu[2]; // On host, for copy back & write.

    // Device memory.
    oskar_VisBlock* vis_block; // Device memory block.
    oskar_Mem *u, *v, *w;
    oskar_Sky* sky_chunk; // The unmodified sky chunk being processed.
    oskar_Sky* local_sky; // A copy of the sky chunk after horizon clipping.
    oskar_Telescope* tel; // Telescope model, created as a copy.
    oskar_Jones *J, *R, *E, *K, *Z;
    oskar_StationWork* station_work;

    // Timers.
    oskar_Timer* tmr;
    oskar_Timer* tmr_write;
    oskar_Timer* tmr_init_copy;
    oskar_Timer* tmr_clip;
    oskar_Timer* tmr_correlate;
    oskar_Timer* tmr_join;
    oskar_Timer* tmr_E;
    oskar_Timer* tmr_K;
};

struct OutputHandles
{
    oskar_VisHeader* header;
    oskar_MeasurementSet* ms;
    oskar_Binary* vis;
    oskar_Mem* temp;
};

///////////////////////////////////////////////////////////////////////////////

static void record_timing_(int num_devices, int* cuda_device_ids,
        DeviceData* d, oskar_Log* log)
{
    double elapsed, t_init = 0.0, t_clip = 0.0, t_E = 0.0, t_K = 0.0;
    double t_join = 0.0, t_correlate = 0.0, t_compute = 0.0;

    // Get component times.
    elapsed = oskar_timer_elapsed(d[0].tmr);
    for (int i = 0; i < num_devices; ++i)
    {
        cudaSetDevice(cuda_device_ids[i]);
        t_init += oskar_timer_elapsed(d[i].tmr_init_copy);
        t_clip += oskar_timer_elapsed(d[i].tmr_clip);
        t_E += oskar_timer_elapsed(d[i].tmr_E);
        t_K += oskar_timer_elapsed(d[i].tmr_K);
        t_join += oskar_timer_elapsed(d[i].tmr_join);
        t_correlate += oskar_timer_elapsed(d[i].tmr_correlate);
    }
    t_compute = t_init + t_clip + t_E + t_K + t_join + t_correlate;
    t_compute /= num_devices;

    // Record time taken.
    oskar_log_section(log, 'M', "Time taken "
            "(wall: %.2f, compute: %.3f, write: %.1f) sec.",
            elapsed, t_compute, oskar_timer_elapsed(d[0].tmr_write));

    // Calculate component percentage times.
    t_init *= (100.0 / (num_devices * elapsed));
    t_clip *= (100.0 / (num_devices * elapsed));
    t_E *= (100.0 / (num_devices * elapsed));
    t_K *= (100.0 / (num_devices * elapsed));
    t_join *= (100.0 / (num_devices * elapsed));
    t_correlate *= (100.0 / (num_devices * elapsed));
    // Using depth = -1 for a line without a bullet.
    oskar_log_message(log, 'M', -1, "%6.1f%% Chunk copy & initialise.", t_init);
    oskar_log_message(log, 'M', -1, "%6.1f%% Horizon clip.", t_clip);
    oskar_log_message(log, 'M', -1, "%6.1f%% Jones E.", t_E);
    oskar_log_message(log, 'M', -1, "%6.1f%% Jones K.", t_K);
    oskar_log_message(log, 'M', -1, "%6.1f%% Jones join.", t_join);
    oskar_log_message(log, 'M', -1, "%6.1f%% Jones correlate.", t_correlate);
}

static void log_warning_box_(oskar_Log* log, const char* format, ...)
{
    size_t max_len = 55; // Controls the width of the box

    char buf[5000];
    va_list args;
    va_start(args, format);
    vsprintf(buf, format, args);
    std::string msg(buf);
    std::istringstream ss(msg);
    std::string word, line;
    oskar_log_line(log, 'W', ' ');
    oskar_log_line(log, 'W', '*');
    while (std::getline(ss, word, ' ')) {
        if (line.length() > 0)
            line += std::string(1, ' ');
        if ((line.length() + word.length() + 4) >= max_len) {
            int pad = max_len - line.length() - 1;
            int pad_l = (pad / 2) > 1 ? (pad / 2) : 1;
            int pad_r = (pad / 2) > 0 ? (pad / 2) : 0;
            if (pad % 2 == 0)
                pad_r -= 1;
            line = "!" + std::string(pad_l, ' ') + line;
            line += std::string(pad_r, ' ') + "!";
            oskar_log_message(log, 'W', -1, "%s", line.c_str());
            line.clear();
        }
        line += word;
    }
    int pad = max_len - line.length() - 1;
    int pad_l = (pad / 2) > 1 ? (pad / 2) : 1;
    int pad_r = (pad / 2) > 0 ? (pad / 2) : 0;
    if (pad % 2 == 0)
        pad_r -= 1;
    line = "!" + std::string(pad_l, ' ') + line;
    line += std::string(pad_r, ' ') + "!";
    oskar_log_message(log, 'W', -1, "%s", line.c_str());
    oskar_log_line(log, 'W', '*');
    oskar_log_line(log, 'W', ' ');
}

static void set_up_device_data_(DeviceData* d, const oskar_Settings* s,
        const oskar_Telescope* tel, int max_sources_per_chunk,
        int num_times_per_block, int* status)
{
    // Obtain local variables with data dimensions.
    int num_stations        = oskar_telescope_num_stations(tel);
    int num_src             = max_sources_per_chunk;
    int num_channels        = s->obs.num_channels;
    int write_autocorr      = 0; // TODO Get from settings.

    // Obtain local variables for data type.
    int prec    = oskar_telescope_precision(tel);
    int complx  = prec | OSKAR_COMPLEX;
    int vistype = complx;
    if (oskar_telescope_pol_mode(tel) == OSKAR_POL_MODE_FULL)
        vistype |= OSKAR_MATRIX;

    // Host memory.
    d->vis_block_cpu[0] = oskar_vis_block_create(vistype, OSKAR_CPU,
            num_times_per_block, num_channels, num_stations, write_autocorr,
            status);
    d->vis_block_cpu[1] = oskar_vis_block_create(vistype, OSKAR_CPU,
            num_times_per_block, num_channels, num_stations, write_autocorr,
            status);

    // Device memory.
    d->vis_block = oskar_vis_block_create(vistype, OSKAR_GPU,
            num_times_per_block, num_channels, num_stations, write_autocorr,
            status);
    d->u = oskar_mem_create(prec, OSKAR_GPU, num_stations, status);
    d->v = oskar_mem_create(prec, OSKAR_GPU, num_stations, status);
    d->w = oskar_mem_create(prec, OSKAR_GPU, num_stations, status);

    d->sky_chunk = oskar_sky_create(prec, OSKAR_GPU, num_src, status);
    d->local_sky = oskar_sky_create(prec, OSKAR_GPU, num_src, status);
    d->tel = oskar_telescope_create_copy(tel, OSKAR_GPU, status);
    d->J = oskar_jones_create(vistype, OSKAR_GPU, num_stations, num_src, status);
    d->R = oskar_mem_type_is_matrix(vistype) ? oskar_jones_create(vistype,
            OSKAR_GPU, num_stations, num_src, status) : 0;
    d->E = oskar_jones_create(vistype, OSKAR_GPU, num_stations, num_src, status);
    d->K = oskar_jones_create(complx, OSKAR_GPU, num_stations, num_src, status);
    d->Z = 0;
    d->station_work = oskar_station_work_create(prec, OSKAR_GPU, status);

    // Timers.
    d->tmr           = oskar_timer_create(OSKAR_TIMER_NATIVE);
    d->tmr_write     = oskar_timer_create(OSKAR_TIMER_NATIVE);
    d->tmr_init_copy = oskar_timer_create(OSKAR_TIMER_CUDA);
    d->tmr_clip      = oskar_timer_create(OSKAR_TIMER_CUDA);
    d->tmr_E         = oskar_timer_create(OSKAR_TIMER_CUDA);
    d->tmr_K         = oskar_timer_create(OSKAR_TIMER_CUDA);
    d->tmr_join      = oskar_timer_create(OSKAR_TIMER_CUDA);
    d->tmr_correlate = oskar_timer_create(OSKAR_TIMER_CUDA);
}

static void free_device_data_(DeviceData* d, int* status)
{
    oskar_vis_block_free(d->vis_block_cpu[0], status);
    oskar_vis_block_free(d->vis_block_cpu[1], status);
    oskar_vis_block_free(d->vis_block, status);
    oskar_mem_free(d->u, status);
    oskar_mem_free(d->v, status);
    oskar_mem_free(d->w, status);
    oskar_sky_free(d->sky_chunk, status);
    oskar_sky_free(d->local_sky, status);
    oskar_telescope_free(d->tel, status);
    oskar_station_work_free(d->station_work, status);
    oskar_jones_free(d->J, status);
    oskar_jones_free(d->E, status);
    oskar_jones_free(d->K, status);
    oskar_jones_free(d->R, status);
    oskar_timer_free(d->tmr);
    oskar_timer_free(d->tmr_write);
    oskar_timer_free(d->tmr_init_copy);
    oskar_timer_free(d->tmr_clip);
    oskar_timer_free(d->tmr_E);
    oskar_timer_free(d->tmr_K);
    oskar_timer_free(d->tmr_join);
    oskar_timer_free(d->tmr_correlate);
}

/*
 * Simulates one slice of a visibility block.
 * The simulation time index is required for random number generators.
 */
static void sim_baselines_(DeviceData* d, oskar_Sky* sky,
        const oskar_Settings* settings, int channel_index_block,
        int time_index_block, int time_index_simulation, int* status)
{
    // Get a handle to the visibility block.
    oskar_VisBlock* blk = d->vis_block;

    // Get dimensions.
    int num_baselines   = oskar_telescope_num_baselines(d->tel);
    int num_stations    = oskar_telescope_num_stations(d->tel);
    int num_src         = oskar_sky_num_sources(sky);
    int num_times_block = oskar_vis_block_num_times(blk);
    int num_channels    = oskar_vis_block_num_channels(blk);

    // Return if there are no sources in the chunk.
    if (num_src == 0) return;

    // Return if the block time index requested is outside the valid range.
    if (time_index_block >= num_times_block) return;

    // Get the time and frequency of the visibility slice being simulated.
    oskar_timer_resume(d->tmr_init_copy);
    double start_time_block = oskar_vis_block_time_start_mjd_utc(blk);
    double end_time_block = oskar_vis_block_time_end_mjd_utc(blk);
    double time_inc = (end_time_block - start_time_block) / num_times_block;
    double t_mjd = start_time_block + time_inc * (time_index_block + 0.5);
    double gast = oskar_convert_mjd_to_gast_fast(t_mjd);

    double start_freq = oskar_vis_block_freq_start_hz(blk);
    double end_freq   = oskar_vis_block_freq_end_hz(blk);
    double freq_inc   = (end_freq - start_freq) / num_channels;
    double frequency  = start_freq + channel_index_block * freq_inc;

    // Scale sky fluxes with spectral index and rotation measure.
    oskar_sky_scale_flux_with_frequency(sky, frequency, status);

    // Pull visibility pointer out for this time and channel.
    oskar_Mem* xcorr = oskar_mem_create_alias(
            oskar_vis_block_cross_correlations(blk), num_baselines *
            (num_channels * time_index_block + channel_index_block),
            num_baselines, status);

    // Evaluate station u,v,w coordinates.
    const oskar_Mem *x, *y, *z;
    double ra0 = oskar_telescope_phase_centre_ra_rad(d->tel);
    double dec0 = oskar_telescope_phase_centre_dec_rad(d->tel);
    x = oskar_telescope_station_true_x_offset_ecef_metres_const(d->tel);
    y = oskar_telescope_station_true_y_offset_ecef_metres_const(d->tel);
    z = oskar_telescope_station_true_z_offset_ecef_metres_const(d->tel);
    oskar_convert_ecef_to_station_uvw(num_stations, x, y, z, ra0, dec0, gast,
            d->u, d->v, d->w, status);

    // Set dimensions of Jones matrices.
    if (d->R)
        oskar_jones_set_size(d->R, num_stations, num_src, status);
    if (d->Z)
        oskar_jones_set_size(d->Z, num_stations, num_src, status);
    oskar_jones_set_size(d->J, num_stations, num_src, status);
    oskar_jones_set_size(d->E, num_stations, num_src, status);
    oskar_jones_set_size(d->K, num_stations, num_src, status);
    oskar_timer_pause(d->tmr_init_copy);

    // Evaluate station beam (Jones E: may be matrix).
    oskar_timer_resume(d->tmr_E);
    oskar_evaluate_jones_E(d->E, num_src, oskar_sky_l(sky), oskar_sky_m(sky),
            oskar_sky_n(sky), OSKAR_RELATIVE_DIRECTIONS,
            oskar_sky_reference_ra_rad(sky), oskar_sky_reference_dec_rad(sky),
            d->tel, gast, frequency, d->station_work, time_index_simulation,
            status);
    oskar_timer_pause(d->tmr_E);

#if 0
    // Evaluate ionospheric phase screen (Jones Z: scalar),
    // and join with Jones E.
    // NOTE this is currently only a CPU implementation.
    if (d->Z)
    {
        oskar_evaluate_jones_Z(d->Z, num_src, sky, d->tel,
                &settings->ionosphere, gast, frequency, &(d->workJonesZ),
                status);
        oskar_timer_resume(d->tmr_join);
        oskar_jones_join(d->E, d->Z, d->E, status);
        oskar_timer_pause(d->tmr_join);
    }
#endif

    // Evaluate parallactic angle (Jones R: matrix), and join with Jones Z*E.
    // TODO Move this into station beam evaluation instead.
    if (d->R)
    {
        oskar_timer_resume(d->tmr_join);
        oskar_evaluate_jones_R(d->R, num_src, oskar_sky_ra_rad_const(sky),
                oskar_sky_dec_rad_const(sky), d->tel, gast, status);
        oskar_jones_join(d->R, d->E, d->R, status);
        oskar_timer_pause(d->tmr_join);
    }

    // Evaluate interferometer phase (Jones K: scalar).
    oskar_timer_resume(d->tmr_K);
    oskar_evaluate_jones_K(d->K, num_src, oskar_sky_l_const(sky),
            oskar_sky_m_const(sky), oskar_sky_n_const(sky), d->u, d->v, d->w,
            frequency, oskar_sky_I_const(sky),
            settings->sky.common_flux_filter_min_jy,
            settings->sky.common_flux_filter_max_jy, status);
    oskar_timer_pause(d->tmr_K);

    // Join Jones K with Jones Z*E*R (if it exists), otherwise with Jones Z*E
    oskar_timer_resume(d->tmr_join);
    oskar_jones_join(d->J, d->K, d->R ? d->R : d->E, status);
    oskar_timer_pause(d->tmr_join);

    // Correlate.
    oskar_timer_resume(d->tmr_correlate);
    oskar_correlate(xcorr, num_src, d->J, sky, d->tel, d->u, d->v, d->w,
            gast, frequency, status);
    oskar_timer_pause(d->tmr_correlate);

    // Free handle to aliased memory.
    oskar_mem_free(xcorr, status);
}

static void sim_vis_block_(const oskar_Settings* s, DeviceData* d,
        int num_gpus, int gpu_id, int total_chunks,
        const oskar_Sky* const* sky_chunks, int block_index, int iactive,
        oskar_Log* log, int* status)
{
    // Clear the visibility block.
    oskar_VisBlock* vis_block = d->vis_block;
    oskar_vis_block_clear(vis_block, status);

    // Initialise the visibility block meta-data.
    int block_length = s->interferometer.max_time_samples_per_block;
    double obs_start_mjd = s->obs.start_mjd_utc;
    double dt_dump = s->obs.dt_dump_days;
    int total_times = s->obs.num_time_steps;
    int block_start_time_index = block_index * block_length;
    int block_end_time_index = block_start_time_index + block_length - 1;
    if (block_end_time_index >= total_times)
        block_end_time_index = total_times - 1;
    int num_times_block = 1 + block_end_time_index - block_start_time_index;
    double block_start_time_mjd = obs_start_mjd +
            block_start_time_index * dt_dump;
    double block_end_time_mjd   = obs_start_mjd +
            (block_end_time_index + 1) * dt_dump;
    int num_channels = s->obs.num_channels;
    double freq_inc = s->obs.frequency_inc_hz;
    double start_freq = s->obs.start_frequency_hz;
    double end_freq = start_freq + num_channels * freq_inc;
    // Set the number of active times in the block
    oskar_vis_block_set_num_times(vis_block, num_times_block, status);
    // Set the time range of the block (of active times slices only)
    oskar_vis_block_set_time_range_mjd_utc(vis_block,
            block_start_time_mjd, block_end_time_mjd);
    // Set the active frequency range of the block.
    oskar_vis_block_set_freq_range_hz(vis_block, start_freq, end_freq);
    if (*status) return;

    // Get time and chunk counter ranges for different parallelisation modes.
    int num_chunks, start_chunk, start_time, num_times;
    if (s->sim.splitting_mode == OSKAR_SPLIT_CHUNK)
    {
        start_time = 0;
        num_times = block_length;
        oskar_round_robin(total_chunks, num_gpus, gpu_id,
                &num_chunks, &start_chunk);
    }
    else
    {
        start_chunk = 0;
        num_chunks = total_chunks;
        oskar_round_robin(block_length, num_gpus, gpu_id,
                &num_times, &start_time);
    }

    for (int c = start_chunk; c < start_chunk + num_chunks; ++c)
    {
        if (*status) break;
        oskar_Sky* sky = d->sky_chunk;

        // Copy the current sky chunk to the GPU.
        // NOTE this is potentially a bit wasteful, as all relevant sky
        // chunks are copied to the GPU for each vis block.
        // Extra no. vis blocks memory copies of each sky chunk.
        oskar_timer_resume(d->tmr_init_copy);
        oskar_sky_copy(sky, sky_chunks[c], status);
        oskar_timer_pause(d->tmr_init_copy);

        for (int t = start_time; t < (start_time + num_times); ++t)
        {
            if (*status || t >= num_times_block) break;
            int time_idx = block_index * block_length + t;
            if (s->sky.apply_horizon_clip)
            {
                double mjd = obs_start_mjd + dt_dump * (time_idx + 0.5);
                double gast = oskar_convert_mjd_to_gast_fast(mjd);
                sky = d->local_sky;
                oskar_timer_resume(d->tmr_clip);
                oskar_sky_horizon_clip(sky, d->sky_chunk,
                        d->tel, gast, d->station_work, status);
                oskar_timer_pause(d->tmr_clip);
            }

            for (int f = 0; f < num_channels; ++f)
            {
                if (*status) break;
                oskar_log_message(log, 'S', 1, "Time %6d/%d, "
                        "Chunk %3d/%d, Channel %4d/%d [dev.%d, %d sources]",
                        time_idx+1, total_times, c+1, num_chunks,
                        f+1, num_channels, gpu_id, oskar_sky_num_sources(sky));
                sim_baselines_(d, sky, s, f, t, time_idx, status);
            }
        }
    }

    // Copy the visibility block to host memory.
    oskar_timer_resume(d->tmr_init_copy);
    oskar_vis_block_copy(d->vis_block_cpu[iactive], d->vis_block, status);
    oskar_timer_pause(d->tmr_init_copy);
}


static void write_vis_block_(const oskar_Settings* s, DeviceData* d,
        int num_gpus, OutputHandles* out, const oskar_Telescope* tel,
        int block_index, int iactive, int* status)
{
    // Can't safely do GPU operations in here, even if cudaSetDevice()
    // is called, because we don't want to block the default stream.
    // CUDA streams could help here, but for now, ensure we do the copy back
    // at the end of the block simulation.

    // Un-pause write timer.
    oskar_timer_resume(d[0].tmr_write);

    // Combine all vis blocks into the first one.
    oskar_VisBlock* blk = d[0].vis_block_cpu[!iactive];
    oskar_Mem* xcorr0 = oskar_vis_block_cross_correlations(blk);
    oskar_Mem* acorr0 = oskar_vis_block_auto_correlations(blk);
    for (int i = 1; i < num_gpus; ++i)
    {
        oskar_VisBlock* b = d[i].vis_block_cpu[!iactive];
        oskar_mem_add(xcorr0, xcorr0,
                oskar_vis_block_cross_correlations(b), status);
        oskar_mem_add(acorr0, acorr0,
                oskar_vis_block_auto_correlations(b), status);
    }

    // Calculate baseline uvw coordinates for vis block.
    double t_start = oskar_vis_block_time_start_mjd_utc(blk);
    double t_end = oskar_vis_block_time_end_mjd_utc(blk);
    int num_times = oskar_vis_block_num_times(blk);
    double dt_dump_sec = (t_end - t_start) / num_times;
    oskar_convert_ecef_to_baseline_uvw(
            oskar_telescope_num_stations(tel),
            oskar_telescope_station_measured_x_offset_ecef_metres_const(tel),
            oskar_telescope_station_measured_y_offset_ecef_metres_const(tel),
            oskar_telescope_station_measured_z_offset_ecef_metres_const(tel),
            oskar_telescope_phase_centre_ra_rad(tel),
            oskar_telescope_phase_centre_dec_rad(tel),
            num_times, t_start, dt_dump_sec,
            oskar_vis_block_baseline_uu_metres(blk),
            oskar_vis_block_baseline_vv_metres(blk),
            oskar_vis_block_baseline_ww_metres(blk), out->temp, status);

    // Add uncorrelated system noise to the combined visibilities.
    if (s->interferometer.noise.enable)
        oskar_vis_block_add_system_noise(blk, tel,
                s->interferometer.noise.seed, block_index, out->temp, status);

    // Write the combined vis block into the MS.
#ifndef OSKAR_NO_MS
    const char* ms_name = s->interferometer.ms_filename;
    if (ms_name && !*status)
    {
        if (block_index == 0)
        {
            bool overwrite = true;
            bool force_polarised = s->interferometer.force_polarised_ms;
            out->ms = oskar_vis_header_write_ms(out->header, ms_name,
                    overwrite, force_polarised, status);
        }
        oskar_vis_block_write_ms(blk, out->header, out->ms, status);
    }
#endif
    // Write the combined vis block into the OSKAR vis binary file.
    const char* vis_name = s->interferometer.oskar_vis_filename;
    if (vis_name && !*status)
    {
        if (block_index == 0)
            out->vis = oskar_vis_header_write(out->header, vis_name, status);
        oskar_vis_block_write(blk, out->vis, block_index, status);
    }

    // Pause write timer.
    oskar_timer_pause(d[0].tmr_write);
}

///////////////////////////////////////////////////////////////////////////////

extern "C" void oskar_sim_interferometer_new(const char* settings_file,
        oskar_Log* log, int* status)
{
    // Load the settings file.
    oskar_Settings s;
    oskar_log_section(log, 'M', "Loading settings file '%s'", settings_file);
    oskar_settings_load(&s, log, settings_file, status);
    if (*status) return;

    // Log the relevant settings. (TODO fix/automate these functions)
    oskar_log_set_keep_file(log, s.sim.keep_log_file);
    oskar_log_set_file_priority(log, s.sim.write_status_to_log_file ?
            OSKAR_LOG_STATUS : OSKAR_LOG_MESSAGE);
    oskar_log_settings_simulator(log, &s);
    oskar_log_settings_sky(log, &s);
    oskar_log_settings_observation(log, &s);
    oskar_log_settings_telescope(log, &s);
    oskar_log_settings_interferometer(log, &s);

    // Check that an output data file has been specified.
    const char* vis_name = s.interferometer.oskar_vis_filename;
    const char* ms_name = s.interferometer.ms_filename;
    if (!(vis_name || ms_name))
    {
        oskar_log_error(log, "No output file specified.");
        *status = OSKAR_ERR_SETTINGS;
        return;
    }

    // Get the number of requested GPUs.
    // If OpenMP is not available, this can only be 1.
    int num_gpus = s.sim.num_cuda_devices;
    int num_threads = 1;
#ifdef _OPENMP
    num_threads = num_gpus + 1;
    omp_set_num_threads(num_threads);
#else
    num_gpus = 1;
    oskar_log_warning(log, "OpenMP not available: Ignoring CUDA device list.");
#endif

    // Find out how many GPUs are in the system.
    int num_gpus_avail = 0;
    *status = (int)cudaGetDeviceCount(&num_gpus_avail);
    if (*status) return;
    if (num_gpus_avail < num_gpus)
    {
        oskar_log_error(log, "More CUDA devices were requested than found.");
        *status = OSKAR_ERR_CUDA_DEVICES;
        return;
    }

    // Set up telescope model and sky model chunk array.
    int num_chunks = 0;
    oskar_Sky** sky_chunks = oskar_set_up_sky(&s, log, &num_chunks, status);
    oskar_Telescope* tel = oskar_set_up_telescope(&s, log, status);

    // Check for errors to ensure there are no null pointers.
    if (*status) return;

    // Work out how many time blocks have to be processed.
    int total_times = s.obs.num_time_steps;
    int block_length = s.interferometer.max_time_samples_per_block;
    int num_time_blocks = (total_times + block_length - 1) / block_length;

    // Initialise each of the requested GPUs and set up per-GPU memory.
    std::vector<DeviceData> d(num_gpus);
    for (int i = 0; i < num_gpus; ++i)
    {
        *status = (int)cudaSetDevice(s.sim.cuda_device_ids[i]);
        if (*status) return;
        cudaDeviceSynchronize();
        set_up_device_data_(&d[i], &s, tel, s.sim.max_sources_per_chunk,
                block_length, status);
    }

    // Create output file-handle structure and visibility header.
    int prec = s.sim.double_precision ? OSKAR_DOUBLE : OSKAR_SINGLE;
    OutputHandles out;
    out.vis = 0;
    out.ms = 0;
    out.header = oskar_set_up_vis_header(&s, tel, status);
    out.temp = oskar_mem_create(prec, OSKAR_CPU, 0, status);

    // Start simulation timer.
    oskar_log_section(log, 'M', "Starting simulation...");
    oskar_timer_start(d[0].tmr);

    //--------------------------------------------------------------------------
    //-- START OF MULTITHREADED SIMULATION CODE --------------------------------
    //--------------------------------------------------------------------------
    /* Loop over blocks of observation time, running simulation and file
     * writing one block at a time. Simulation and file output are overlapped
     * by using double buffering, and a dedicated thread is used for file
     * output.
     *
     * Thread 0 is used for file writes.
     * Threads 1 to n (mapped to GPUs) execute the simulation.
     *
     * Note that no write is launched on the first loop counter (as no
     * data are ready yet) and no simulation is performed for the last loop
     * counter (which corresponds to the last block + 1) as this iteration
     * simply writes the last block.
     */
#pragma omp parallel
    {
        int thread_id = 0, gpu_id = 0;

        // Get the host thread ID, and set the CUDA device used by this thread.
#ifdef _OPENMP
        thread_id = omp_get_thread_num();
        gpu_id = thread_id - 1;
#endif
        if (gpu_id >= 0)
            cudaSetDevice(s.sim.cuda_device_ids[gpu_id]);

        // Loop over simulation time blocks (+1, for the last write).
        for (int b = 0; b < num_time_blocks + 1; ++b)
        {
            if (*status) continue;
            int iactive = b % 2; // Index of the active simulation vis block.

            if ((thread_id > 0 || num_threads == 1) && b < num_time_blocks)
            {
                sim_vis_block_(&s, &d[gpu_id], num_gpus, gpu_id,
                        num_chunks, sky_chunks, b, iactive, log, status);
                oskar_log_message(log, 'S', 0, "Block %d/%d (%.1f%%) "
                        "completed after %.2f sec.", b+1, num_time_blocks,
                        (100.0 * (b + 1.0)) / num_time_blocks,
                        oskar_timer_elapsed(d[0].tmr));
                oskar_cuda_mem_log(log, 0, gpu_id);
            }
            if (thread_id == 0 && b > 0)
                write_vis_block_(&s, &d[0], num_gpus, &out,
                        tel, b - 1, iactive, status);

            // Barrier: Check sim and write are done before starting new block.
#pragma omp barrier
        }
    }
    //--------------------------------------------------------------------------
    //-- END OF MULTITHREADED SIMULATION CODE ----------------------------------
    //--------------------------------------------------------------------------

    // Record times.
    record_timing_(num_gpus, s.sim.cuda_device_ids, &(d[0]), log);

    // If there are sources in the simulation and the station beam is not
    // normalised to 1.0 at the phase centre, the values of noise RMS
    // may give a very unexpected S/N ratio!
    // The alternative would be to scale the noise to match the station
    // beam gain but that would require knowledge of the station beam
    // amplitude at the phase centre for each time and channel.
    if (s.interferometer.noise.enable)
    {
        int have_sources = (num_chunks > 0 &&
                oskar_sky_num_sources(sky_chunks[0]) > 0);
        int amp_calibrated = s.telescope.normalise_beams_at_phase_centre;
        if (have_sources && !amp_calibrated)
            log_warning_box_(log, "WARNING: System noise was added to "
                    "visibilities without station beam normalisation enabled. "
                    "This may lead to an invalid signal to noise ratio.");
    }

    // Write simulation log to the output files.
    size_t log_size = 0;
    char* log_data = oskar_log_file_data(log, &log_size);
#ifndef OSKAR_NO_MS
    if (out.ms)
        oskar_ms_add_log(out.ms, log_data, log_size);
#endif
    if (out.vis)
        oskar_binary_write(out.vis, OSKAR_CHAR, OSKAR_TAG_GROUP_RUN,
                OSKAR_TAG_RUN_LOG, 0, log_size, log_data, status);
    free(log_data);

    // Free device memory.
    for (int i = 0; i < num_gpus; ++i)
    {
        cudaSetDevice(s.sim.cuda_device_ids[i]);
        free_device_data_(&d[i], status);
        cudaDeviceReset();
    }

    // Free host memory.
    for (int i = 0; i < num_chunks; ++i) oskar_sky_free(sky_chunks[i], status);
    free(sky_chunks);
    oskar_telescope_free(tel, status);

    // Free/close output handles
    oskar_vis_header_free(out.header, status);
    oskar_mem_free(out.temp, status);
    oskar_binary_free(out.vis);
#ifndef OSKAR_NO_MS
    oskar_ms_close(out.ms);
#endif

    if (!*status)
        oskar_log_section(log, 'M', "Run complete.");
}
