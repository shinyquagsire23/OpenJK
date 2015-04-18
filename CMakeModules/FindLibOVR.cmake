# Locate LIBOVR library
# This module defines
# LIBOVR_LIBRARY, the name of the library to link against
# LIBOVR_FOUND, if false, do not try to link to LIBOVR
# LIBOVR_INCLUDE_DIR, where to find SDL.h
#
# This module responds to the the flag:
# LIBOVR_BUILDING_LIBRARY
# If this is defined, then no LIBOVRmain will be linked in because
# only applications need main().
# Otherwise, it is assumed you are building an application and this
# module will attempt to locate and set the the proper link flags
# as part of the returned LIBOVR_LIBRARY variable.
#
# Don't forget to include SDLmain.h and SDLmain.m your project for the
# OS X framework based version. (Other versions link to -lLIBOVRmain which
# this module will try to find on your behalf.) Also for OS X, this
# module will automatically add the -framework Cocoa on your behalf.
#
#
#
#
# $LIBOVRDIR is an environment variable that would
# correspond to the ./configure --prefix=$LIBOVRDIR
# used in building LIBOVR.
# l.e.galup  9-20-02
#
# Modified by Eric Wing.
# Added code to assist with automated building by using environmental variables
# and providing a more controlled/consistent search behavior.
# Added new modifications to recognize OS X frameworks and
# additional Unix paths (FreeBSD, etc).
# Also corrected the header search path to follow "proper" SDL guidelines.
# Added a search for LIBOVRmain which is needed by some platforms.
# Added a search for threads which is needed by some platforms.
# Added needed compile switches for MinGW.
#
# On OSX, this will prefer the Framework version (if found) over others.
# People will have to manually change the cache values of
# LIBOVR_LIBRARY to override this selection or set the CMake environment
# CMAKE_INCLUDE_PATH to modify the search paths.
#
# Note that the header path has changed from LIBOVR/SDL.h to just SDL.h
# This needed to change because "proper" SDL convention
# is #include "SDL.h", not <LIBOVR/SDL.h>. This is done for portability
# reasons because not all systems place things in LIBOVR/ (see FreeBSD).

#=============================================================================
# Copyright 2003-2009 Kitware, Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

SET(LIBOVR_SEARCH_PATHS
	/home/maxamillion/workspace/ovr_sdk_linux_0.4.4/LibOVR
	#~/Library/Frameworks
	#/Library/Frameworks
	#/usr/local
	#/usr
	#/sw # Fink
	#/opt/local # DarwinPorts
	#/opt/csw # Blastwave
	#/opt
)

FIND_PATH(LIBOVR_INCLUDE_DIR OVR_CAPI.h
	NAMES OVR_CAPI.h
	HINTS
	$ENV{LIBOVRDIR}
	PATH_SUFFIXES include/ovr-0.4.3 include
	PATHS ${LIBOVR_SEARCH_PATHS}
)

set(LIBOVR_INCLUDE_DIR /home/maxamillion/workspace/ovr_sdk_linux_0.4.4/LibOVR/src)
set(LIBOVR_NAMES ${LIBOVR_NAMES} libovr.so libovr.dll)
find_library(LIBOVR_LIBRARY
	NAMES
		${LIBOVR_NAMES}
	HINTS
	PATH_SUFFIXES
		lib64
		lib
	PATHS
		/sw
		/opt/local
		/opt/csw
		/opt
		/usr
	)

IF(NOT LIBOVR_BUILDING_LIBRARY)
	IF(NOT ${LIBOVR_INCLUDE_DIR} MATCHES ".framework")
		# Non-OS X framework versions expect you to also dynamically link to
		# LIBOVRmain. This is mainly for Windows and OS X. Other (Unix) platforms
		# seem to provide LIBOVRmain for compatibility even though they don't
		# necessarily need it.
		FIND_LIBRARY(LIBOVRMAIN_LIBRARY
			NAMES LIBOVRmain
			HINTS
			$ENV{LIBOVRDIR}
			PATH_SUFFIXES lib64 lib
			PATHS ${LIBOVR_SEARCH_PATHS}
		)
	ENDIF(NOT ${LIBOVR_INCLUDE_DIR} MATCHES ".framework")
ENDIF(NOT LIBOVR_BUILDING_LIBRARY)

# LIBOVR may require threads on your system.
# The Apple build may not need an explicit flag because one of the
# frameworks may already provide it.
# But for non-OSX systems, I will use the CMake Threads package.
IF(NOT APPLE)
	FIND_PACKAGE(Threads)
ENDIF(NOT APPLE)

# MinGW needs an additional library, mwindows
# It's total link flags should look like -lmingw32 -lLIBOVRmain -lLIBOVR -lmwindows
# (Actually on second look, I think it only needs one of the m* libraries.)
IF(MINGW)
	SET(MINGW32_LIBRARY mingw32 CACHE STRING "mwindows for MinGW")
ENDIF(MINGW)

IF(LIBOVR_LIBRARY_TEMP)
	# For LIBOVRmain
	IF(NOT LIBOVR_BUILDING_LIBRARY)
		IF(LIBOVRMAIN_LIBRARY)
			SET(LIBOVR_LIBRARY_TEMP ${LIBOVRMAIN_LIBRARY} ${LIBOVR_LIBRARY_TEMP})
		ENDIF(LIBOVRMAIN_LIBRARY)
	ENDIF(NOT LIBOVR_BUILDING_LIBRARY)

	# For OS X, LIBOVR uses Cocoa as a backend so it must link to Cocoa.
	# CMake doesn't display the -framework Cocoa string in the UI even
	# though it actually is there if I modify a pre-used variable.
	# I think it has something to do with the CACHE STRING.
	# So I use a temporary variable until the end so I can set the
	# "real" variable in one-shot.
	IF(APPLE)
		SET(LIBOVR_LIBRARY_TEMP ${LIBOVR_LIBRARY_TEMP} "-framework Cocoa")
	ENDIF(APPLE)

	# For threads, as mentioned Apple doesn't need this.
	# In fact, there seems to be a problem if I used the Threads package
	# and try using this line, so I'm just skipping it entirely for OS X.
	IF(NOT APPLE)
		SET(LIBOVR_LIBRARY_TEMP ${LIBOVR_LIBRARY_TEMP} ${CMAKE_THREAD_LIBS_INIT})
	ENDIF(NOT APPLE)

	# For MinGW library
	IF(MINGW)
		SET(LIBOVR_LIBRARY_TEMP ${MINGW32_LIBRARY} ${LIBOVR_LIBRARY_TEMP})
	ENDIF(MINGW)

	# Set the final string here so the GUI reflects the final state.
	SET(LIBOVR_LIBRARY ${LIBOVR_LIBRARY_TEMP} CACHE STRING "Where the LIBOVR Library can be found")
	# Set the temp variable to INTERNAL so it is not seen in the CMake GUI
	SET(LIBOVR_LIBRARY_TEMP "${LIBOVR_LIBRARY_TEMP}" CACHE INTERNAL "")
ENDIF(LIBOVR_LIBRARY_TEMP)

INCLUDE(FindPackageHandleStandardArgs)

FIND_PACKAGE_HANDLE_STANDARD_ARGS(LIBOVR REQUIRED_VARS LIBOVR_LIBRARY LIBOVR_INCLUDE_DIR)
