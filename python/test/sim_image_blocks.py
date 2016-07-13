#!/usr/bin/python
import numpy
import os
import oskar
import time

class MySimulator(oskar.Simulator):
    """Simulates and images visibilities concurrently.

    Inherits oskar.Simulator to image each block in the process_block() method.
    """

    def __init__(self, imagers, precision='double'):
        """Creates the simulator, storing a handle to the imagers.

        Args:
            imagers (oskar.Imager list): List of OSKAR imagers to use.
            precision (str): Either 'double' or 'single' to specify
                the numerical precision of the simulation.
        """
        oskar.Simulator.__init__(self, precision)
        self.imagers = imagers


    def finalise(self):
        """Called automatically by the base class at the end of run_blocks()."""
        oskar.Simulator.finalise(self)
        for i in self.imagers: i.finalise()


    def process_block(self, block, block_index):
        """Writes the visibility block to any open file(s), and images it.

        Args:
            block (oskar.VisBlock): A handle to the block to be processed.
            block_index (int):      The index of the visibility block.
        """
        oskar.Simulator.process_block(self, block, block_index)
        for i in self.imagers: i.update_from_block(self.vis_header(), block)


    def set_coords_only(self, value):
        """Calls set_coords_only() on simulator and imager objects."""
        oskar.Simulator.set_coords_only(self, value)
        for i in self.imagers: i.set_coords_only(value)


if __name__ == '__main__':
    # Global options.
    precision = 'single'
    phase_centre_ra_deg = 0.0
    phase_centre_dec_deg = 60.0
    output_root = 'test_blocks'

    # Define a telescope layout.
    num_stations = 300
    numpy.random.seed(1)
    x = 5000 * numpy.random.randn(num_stations)
    y = 5000 * numpy.random.randn(num_stations)

    # Set up the sky model.
    sky = oskar.Sky.generate_grid(phase_centre_ra_deg, phase_centre_dec_deg,
        16, 1.5, precision=precision)
    sky.append_sources(phase_centre_ra_deg, phase_centre_dec_deg, 1.0)

    # Set up the telescope model.
    tel = oskar.Telescope(precision)
    tel.set_channel_bandwidth(1.0e3)
    tel.set_time_average(10.0)
    tel.set_pol_mode('Scalar')
    tel.set_station_coords_enu(longitude_deg=0, latitude_deg=60, altitude_m=0, 
        x=x, y=y)
    # Set station properties after stations have been defined.
    tel.set_phase_centre(phase_centre_ra_deg, phase_centre_dec_deg)
    tel.set_station_type('Gaussian beam')
    tel.set_gaussian_station_beam_values(5.0, 100e6)

    # Set up two imagers for natural and uniform weighting.
    imagers = []
    for i in range(2):
        imagers.append(oskar.Imager(precision))
        imagers[i].set_size(2048)
        imagers[i].set_fov(2.0)
        imagers[i].set_algorithm('W-projection')
    imagers[0].set_output_root(output_root+'_uniform')
    imagers[0].set_weighting('Uniform')
    imagers[1].set_output_root(output_root+'_natural')
    imagers[1].set_weighting('Natural')

    # Set up the simulator.
    simulator = MySimulator(imagers, precision)
    simulator.set_settings_path(os.path.abspath(__file__))
    simulator.set_sky_model(sky)
    simulator.set_telescope_model(tel)
    simulator.set_observation_frequency(100e6)
    simulator.set_observation_time(start_time_mjd_utc=51545.0,
        length_sec=43200.0, num_time_steps=48)

    # Simulate and image visibilities.
    # Coordinates are needed first for uniform weighting or W-projection.
    start = time.time()
    print('Simulating and imaging...')
    simulator.set_coords_only(True)
    simulator.run_blocks()
    #simulator.set_output_measurement_set(output_root+'.ms')
    simulator.set_coords_only(False)
    simulator.run_blocks()
    print('Completed after %.3f seconds.' % (time.time() - start))
