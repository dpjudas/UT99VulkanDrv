#=============================================================================
# Unreal Audio makefile for Linux.
#
# Revision history:
# 	* Created by Brandon Reinhart 
#=============================================================================

# Unreal directory. (Required by makefile-header.)
UNREAL_DIR	= ../..

# Include global definitions.
include $(UNREAL_DIR)/makefile-header

#-----------------------------------------------------------------------------
# Directories.
#-----------------------------------------------------------------------------

SRC_DIRS	= $(AUDIO_SRC)
BUILD_DIR	= $(AUDIO_BUILD)

#-----------------------------------------------------------------------------
# Compiler/linker options.
#-----------------------------------------------------------------------------

CXX		= $(STD_CXX)
CXXFLAGS	= $(STD_CXXFLAGS) -Werror -fPIC -I$(AUDIO_INC) -I$(ENGINE_INC) \
			-I$(CORE_INC) -DGPackage=GPackageAudio
LDFLAGS		= $(STD_LDFLAGS) -shared -Wl,-soname,$(AUDIO_SONAME)
LIBS		= $(STD_LIBS) $(CORE) -lpthread

#-----------------------------------------------------------------------------
# Files.
#-----------------------------------------------------------------------------

OBJS =				\
	Audio.o			\
	AudioLibrary.o		\
	AudioMixer.o		\
	AudioCoreLinux.o	\
	AudioSubsystem.o	\
	FormatWAV.o

OUT = $(AUDIO)

#-----------------------------------------------------------------------------
# Rules.
#-----------------------------------------------------------------------------

# Use general rules.
include $(UNREAL_DIR)/makefile-general

#-----------------------------------------------------------------------------
# The End.
#-----------------------------------------------------------------------------
