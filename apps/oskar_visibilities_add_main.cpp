/*
 * Copyright (c) 2012, The University of Oxford
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

#include <oskar_global.h>
#include <interferometry/oskar_visibilities_read.h>
#include <interferometry/oskar_visibilities_init.h>
#include <interferometry/oskar_visibilities_write.h>
#include <interferometry/oskar_visibilities_copy.h>
#include <utility/oskar_get_error_string.h>
#include <utility/oskar_mem_add.h>
#include <utility/oskar_mem_clear_contents.h>
#include <apps/lib/oskar_OptionParser.h>

#include <string>
#include <cmath>
#include <iostream>
#include <cfloat>
#include <vector>
#include <iomanip>

using namespace std;

// -----------------------------------------------------------------------------
void set_options(oskar_OptionParser& opt);
bool check_options(oskar_OptionParser& opt, int argc, char** argv);
bool isCompatible(const oskar_Visibilities& vis1, const oskar_Visibilities& vis2);
void add_visibilities(oskar_Visibilities& out, const oskar_Visibilities& in1,
        const oskar_Visibilities& in2);
void print_error(int status, const char* message);
// -----------------------------------------------------------------------------

int main(int argc, char** argv)
{
    // Register options =======================================================
    oskar_OptionParser opt("oskar_visibilities_add");
    set_options(opt);
    if (!check_options(opt, argc, argv))
        return OSKAR_FAIL;

    // Retrieve options ========================================================
    string out_path;
    opt.get("-o")->getString(out_path);
    vector<string> in_files = opt.getInputFiles(2);
    bool verbose = opt.isSet("-q") ? false : true;
    if (verbose) {
        cout << "Output visibility file = " << out_path << endl;
        cout << "Combining the " << in_files.size() << " input files:" << endl;
        for (int i = 0; i < (int)in_files.size(); ++i) {
            cout << "  [" << setw(2) << i << "] " << in_files[i] << endl;
        }
    }

    // Add the data. ==========================================================
    int status = OSKAR_SUCCESS;

    // Load the first visibility structure.
    oskar_Visibilities in1;
    oskar_visibilities_read(&in1, in_files[0].c_str(), &status);
    int amp_type = in1.amplitude.type;
    int num_channels = in1.num_channels;
    int num_times = in1.num_times;
    int num_stations = in1.num_stations;
    if (status != OSKAR_SUCCESS) {
        string msg = "Failed to read visibility data file " + in_files[0];
        print_error(status, msg.c_str());
        return status;
    }

    // Create an output visibility data structure.
    oskar_Visibilities out;
    oskar_visibilities_init(&out, amp_type, OSKAR_LOCATION_CPU, num_channels,
            num_times, num_stations, &status);
    if (status != OSKAR_SUCCESS) {
        print_error(status, "Failed to initialise output visibility structure.");
        return status;
    }

    // Copy the first input visibility data file into the output data structure.
    oskar_visibilities_copy(&out, &in1, &status);
    oskar_mem_clear_contents(&out.settings_path, &status);
    // TODO write some sort of tag into here to indicate this is an
    // accumulated visibility data set...
    if (status != OSKAR_SUCCESS) {
        print_error(status, "Failed to clear settings path.");
        return status;
    }

    // Loop over other visibility files and combine.
    for (int i = 1; i < (int)in_files.size(); ++i)
    {
        oskar_Visibilities in2;
        oskar_visibilities_read(&in2, in_files[i].c_str(), &status);
        if (status != OSKAR_SUCCESS) {
            string msg = "Failed to read visibility data file " + in_files[i];
            print_error(status, msg.c_str());
            return status;
        }
        if (!isCompatible(out, in2))
        {
            cerr << "ERROR: Input visibility data must match!" << endl;
            return status;
        }
        add_visibilities(out, out, in2);
    }
    if (status != OSKAR_SUCCESS) {
        print_error(status, "Failed to read visibility data files.");
        return status;
    }

    // Write output data ======================================================
    oskar_visibilities_write(&out, 0, out_path.c_str(), &status);
    if (status != OSKAR_SUCCESS) {
        print_error(status, "Failed writing output visibility structure to file.");
        return status;
    }

    return status;
}

void print_error(int status, const char* message)
{
    cerr << "ERROR[" << status << "] " << message << endl;
    cerr << "REASON = " << oskar_get_error_string(status) << endl;
}

void add_visibilities(oskar_Visibilities& out, const oskar_Visibilities& in1,
        const oskar_Visibilities& in2)
{
    int status = OSKAR_SUCCESS;
    oskar_mem_add(&out.amplitude, &in1.amplitude, &in2.amplitude, &status);
    if (status != OSKAR_SUCCESS)
        print_error(status, "Visibility amplitude addition failed.");
}


bool isCompatible(const oskar_Visibilities& v1, const oskar_Visibilities& v2)
{
    if (v1.num_channels != v2.num_channels)
        return false;
    if (v1.num_times != v2.num_times)
        return false;
    if (v1.num_stations != v2.num_stations)
        return false;
    if (v1.num_baselines != v2.num_baselines)
        return false;
    if (fabs(v1.freq_start_hz - v2.freq_start_hz) > DBL_EPSILON)
        return false;
    if (fabs(v1.freq_inc_hz - v2.freq_inc_hz) > DBL_EPSILON)
        return false;
    if (fabs(v1.channel_bandwidth_hz - v2.channel_bandwidth_hz) > DBL_EPSILON)
        return false;
    if (fabs(v1.time_start_mjd_utc - v2.time_start_mjd_utc) > DBL_EPSILON)
        return false;
    if (fabs(v1.time_inc_seconds - v2.time_inc_seconds) > DBL_EPSILON)
        return false;
    if (fabs(v1.phase_centre_ra_deg - v2.phase_centre_ra_deg) > DBL_EPSILON)
        return false;
    if (fabs(v1.phase_centre_dec_deg - v2.phase_centre_dec_deg) > DBL_EPSILON)
        return false;

    if (v1.amplitude.type != v2.amplitude.type)
        return false;

    return true;
}

void set_options(oskar_OptionParser& opt)
{
    opt.setDescription("Application to combine OSKAR binary visibility files.");
    opt.addRequired("OSKAR visibility files...");
    opt.addFlag("-o", "Output visibility file name", 1, "out.vis", false, "--output");
    opt.addFlag("-q", "Disable log messages", false, "--quiet");
    opt.addExample("oskar_visibilities_add file1.vis file2.vis");
    opt.addExample("oskar_visibilities_add file1.vis file2.vis -o combined.vis");
    opt.addExample("oskar_visibilities_add -q file1.vis file2.vis file3.vis");
    opt.addExample("oskar_visibilities_add *.vis");
}

bool check_options(oskar_OptionParser& opt, int argc, char** argv)
{
    if (!opt.check_options(argc, argv))
        return false;
    bool visFirst = ((int)opt.firstArgs.size() >= 3) &&
            ((int)opt.lastArgs.size() == 0);
    bool visEnd = ((int)opt.firstArgs.size() == 1) &&
            ((int)opt.lastArgs.size() >= 2);
    if (!visFirst && !visEnd)
    {
        opt.error("Please provide 2 or more visibility files to combine.");
        return false;
    }
    return true;
}
