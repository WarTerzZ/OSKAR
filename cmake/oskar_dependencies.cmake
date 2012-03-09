#
# src/cmake/oskar_dependencies.cmake:
#
#
# Dependencies for liboskar:
#------------------------------------------------------------------------------
# [required]
#   CUDA (>= 4.0)
#
# [optional]
#   CppUnit
#   Qt4
#   Qwt5-qt4
#
# Dependencies for liboskar_ms:
#------------------------------------------------------------------------------
#  casacore
#  CppUnit
#
# Dependencies for liboskar_imaging:
#------------------------------------------------------------------------------
#   FFTW
#   CFitsio
#   CppUnit
#
# Dependencies for liboskar_widgets:
#------------------------------------------------------------------------------
#   Qt4
#   Qwt5
#   CppUnit
#

# ==== OS specific path settings.
if (WIN32)
    # qwt5
    set(QWT_INCLUDES     ${CMAKE_SOURCE_DIR}/../include/qwt-5.2.2/)
    set(QWT_LIBRARY_DIR  ${CMAKE_SOURCE_DIR}/../lib/qwt-5.2.2/)

    # cppunit
    set(CPPUNIT_INCLUDES ${CMAKE_SOURCE_DIR}/../include/cppunit-1.12.1/)
    set(CPPUNIT_LIB_DIR  ${CMAKE_SOURCE_DIR}/../lib/cppunit-1.12.1/)
endif ()

# ==== Find dependencies.
find_package(CUDA 4.0)
if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    find_package(Qt4 4.5.0 COMPONENTS QtCore QtGui QtTest QUIET)
    find_package(Qwt5 QUIET)
endif ()
find_package(OpenMP QUIET)
find_package(CppUnit QUIET)
find_package(FFTW3 QUIET)
find_package(CasaCore QUIET)
find_package(CFitsio QUIET)
find_package(Matlab QUIET)
find_package(CBLAS QUIET)
find_package(LAPACK QUIET)


# ==== Work out which libraries to build.
if (NOT CUDA_FOUND)
    message("================================================================================")
    message("-- WARNING: CUDA not found: "
            "Unable to build main OSKAR library.")
    message("================================================================================")
    set(BUILD_OSKAR OFF)
endif ()

if (NOT MATLAB_FOUND)
    message("================================================================================")
    message("-- WARNING: MATLAB not found: "
            "Unable to compile MATLAB mex functions.")
    message("================================================================================")
endif()

if (NOT QT4_FOUND)
    message("================================================================================")
    message("-- WARNING: Qt4 not found.")
    message("================================================================================")
    set(BUILD_OSKAR_WIDGETS OFF)
    set(BUILD_OSKAR_IMAGING OFF) # FIXME why can't imaging library be built if no Qt?
    set(BUILD_OSKAR_APPS    OFF)
endif()

if (NOT Qwt5_FOUND)
    message("================================================================================")
    message("-- WARNING: Qwt5 not found: "
        "Unable to build plotting widgets library.")
    message("================================================================================")
    set(BUILD_OSKAR_WIDGETS OFF)
endif()

if (NOT CASACORE_FOUND)
    message("================================================================================")
    message("-- WARNING: CasaCore not found: "
        "Unable to build OSKAR Measurement Set library.")
    message("================================================================================")
    set(BUILD_OSKAR_MS OFF)
    add_definitions(-DOSKAR_NO_MS)
endif()

if (NOT FFTW3_FOUND)
    message("================================================================================")
    message("-- WARNING: FFTW3 not found: "
            "Unable to build imaging library.")
    message("================================================================================")
    set(BUILD_OSKAR_IMAGING OFF)
endif ()

if (NOT CPPUNIT_FOUND)
    message("================================================================================")
    message("-- WARNING: CppUnit not found: "
           "Unable to build unit testing binaries.")
    message("================================================================================")
endif()


if (NOT CFITSIO_FOUND)
    message("================================================================================")
    message("-- WARNING: cfitsio not found: "
           "Unable to build FITS library.")
    message("================================================================================")
    set(BUILD_OSKAR_FITS OFF)
    add_definitions(-DOSKAR_NO_FITS)
endif ()

if (NOT CBLAS_FOUND)
    message("================================================================================")
    message("-- WARNING: cblas not found")
    message("================================================================================")
    add_definitions(-DOSKAR_NO_CBLAS)
endif()

if (NOT LAPACK_FOUND)
    message("================================================================================")
    message("-- WARNING: lapack not found")
    message("================================================================================")
    add_definitions(-DOSKAR_NO_LAPACK)
endif()



if (NOT BUILD_OSKAR)
    set(BUILD_OSKAR_PLATFORM OFF)
endif()

# ==== Prints a message saying which libraries are being built.
if (BUILD_OSKAR)
    message("==> Building 'liboskar'")
endif ()
if (BUILD_OSKAR_MS)
    message("==> Building 'liboskar_ms'")
endif ()
if (BUILD_OSKAR_IMAGING)
    message("==> Building 'liboskar_imaging'")
endif ()
if (BUILD_OSKAR_WIDGETS)
    message("==> Building 'liboskar_widgets'")
endif ()
if (BUILD_OSKAR_FITS)
    message("==> Building 'liboskar_fits'")
endif ()
if (BUILD_OSKAR_APPS)
    message("==> Building 'liboskar_apps' and OSKAR applications")
endif ()

# ==== Set a flag to tell cmake that dependencies have been checked.
set(CHECKED_DEPENDENCIES YES)
