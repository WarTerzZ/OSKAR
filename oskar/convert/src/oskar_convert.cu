/* Copyright (c) 2018-2019, The University of Oxford. See LICENSE file. */

#include "convert/define_convert_cirs_relative_directions_to_enu_directions.h"
#include "convert/define_convert_ecef_to_station_uvw.h"
#include "convert/define_convert_enu_directions_to_cirs_relative_directions.h"
#include "convert/define_convert_enu_directions_to_relative_directions.h"
#include "convert/define_convert_enu_directions_to_theta_phi.h"
#include "convert/define_convert_lon_lat_to_relative_directions.h"
#include "convert/define_convert_ludwig3_to_theta_phi_components.h"
#include "convert/define_convert_relative_directions_to_enu_directions.h"
#include "convert/define_convert_station_uvw_to_baseline_uvw.h"
#include "utility/oskar_cuda_registrar.h"
#include "utility/oskar_kernel_macros.h"
#include "utility/oskar_vector_types.h"

/* Kernels */

#define Real float
#define Real2 float2
#include "convert/src/oskar_convert.cl"
#undef Real
#undef Real2

#define Real double
#define Real2 double2
#include "convert/src/oskar_convert.cl"
#undef Real
#undef Real2
