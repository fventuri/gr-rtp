# Copyright 2020 Free Software Foundation, Inc.
#
# This file is part of GNU Radio
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

########################################################################
# Check if there is C++ code at all
########################################################################
if(NOT rtp_sources)
    MESSAGE(STATUS "No C++ sources... skipping python bindings")
    return()
endif(NOT rtp_sources)

########################################################################
# Check for pygccxml
########################################################################
GR_PYTHON_CHECK_MODULE_RAW(
    "pygccxml"
    "import pygccxml"
    PYGCCXML_FOUND
    )

include(GrPybind)

########################################################################
# Python Bindings
########################################################################

list(APPEND rtp_python_files
    source_python.cc python_bindings.cc)

GR_PYBIND_MAKE_OOT(rtp
   ../../..
   gr::rtp
   "${rtp_python_files}")

# copy bindings extension for use in QA test module
add_custom_command(TARGET rtp_python POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:rtp_python>
    ${PROJECT_BINARY_DIR}/test_modules/gnuradio/rtp/
)

install(TARGETS rtp_python DESTINATION ${GR_PYTHON_DIR}/gnuradio/rtp COMPONENT pythonapi)
