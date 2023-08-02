find_package(PkgConfig)

PKG_CHECK_MODULES(PC_GR_RTP gnuradio-rtp)

FIND_PATH(
    GR_RTP_INCLUDE_DIRS
    NAMES gnuradio/rtp/api.h
    HINTS $ENV{RTP_DIR}/include
        ${PC_RTP_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    GR_RTP_LIBRARIES
    NAMES gnuradio-rtp
    HINTS $ENV{RTP_DIR}/lib
        ${PC_RTP_LIBDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
          )

include("${CMAKE_CURRENT_LIST_DIR}/gnuradio-rtpTarget.cmake")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GR_RTP DEFAULT_MSG GR_RTP_LIBRARIES GR_RTP_INCLUDE_DIRS)
MARK_AS_ADVANCED(GR_RTP_LIBRARIES GR_RTP_INCLUDE_DIRS)
