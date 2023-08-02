find_package(PkgConfig)

PKG_CHECK_MODULES(PC_RTP rtp)

FIND_PATH(
    RTP_INCLUDE_DIRS
    NAMES rtp/api.h
    HINTS $ENV{RTP_DIR}/include
        ${PC_RTP_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    RTP_LIBRARIES
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

include("${CMAKE_CURRENT_LIST_DIR}/rtpTarget.cmake")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(RTP DEFAULT_MSG RTP_LIBRARIES RTP_INCLUDE_DIRS)
MARK_AS_ADVANCED(RTP_LIBRARIES RTP_INCLUDE_DIRS)
