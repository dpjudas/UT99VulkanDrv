#=============================================================================
# Unreal Core makefile for Linux.
#
# Revision history:
# 	* Created by Mike Danylchuk
#=============================================================================

# Unreal directory. (Required by makefile-header.)
UNREAL_DIR	= ../..

# Include global definitions.
include $(UNREAL_DIR)/makefile-header

#-----------------------------------------------------------------------------
# Directories.
#-----------------------------------------------------------------------------

SRC_DIRS	= $(XLAUNCH_SRC)
BUILD_DIR	= $(XLAUNCH_BUILD)

#-----------------------------------------------------------------------------
# Compiler/linker options.
#-----------------------------------------------------------------------------

CXX		= $(STD_CXX)

ifeq ($(LINKSTYLE),shared)
CXXFLAGS	= $(STD_CXXFLAGS) -I$(CORE_INC) -I$(ENGINE_INC)
LDFLAGS		= $(STD_LDFLAGS) -export-dynamic
LIBS		= $(STD_LIBS) $(ENGINE) $(CORE) $(XLIBS)
endif

ifeq ($(LINKSTYLE),static)
CXXFLAGS	= $(STD_CXXFLAGS) \
			-I$(XLAUNCH_INC) -I$(CORE_INC) \
			-I$(FIRE_INC) -I$(ENGINE_INC) \
			-I$(IPDRV_INC) -I$(UWEB_INC) \
			-I$(XDRV_INC) -I$(AUDIO_INC) \
			-I$(RENDER_INC) -I$(GLIDEDRV_INC) \
			$(GLIDEINC)
LIBS		= $(STD_LIBS) \
			../../System/GlideDrv.a \
			../../System/Render.a ../../System/Audio.a \
			../../System/XDrv.a ../../System/UWeb.a \
			../../System/IpDrv.a ../../System/Fire.a \
			../../System/Engine.a ../../System/Core.a \
			$(GLIDELIBS) $(XLIBS)
endif

#-----------------------------------------------------------------------------
# Files.
#-----------------------------------------------------------------------------

OBJS =		\
	XLaunch.o

OUT = $(XLAUNCH)

#-----------------------------------------------------------------------------
# Rules.
#-----------------------------------------------------------------------------

# Use general rules.
EXECUTABLE	= 1
include $(UNREAL_DIR)/makefile-general

#-----------------------------------------------------------------------------
# The End.
#-----------------------------------------------------------------------------
