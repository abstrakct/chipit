# Generic Makefile gotten from https://github.com/mbcrawfo/GenericMakefile
#
# Adapted to my own needs.


#### PROJECT SETTINGS ####
# The name of the executable to be created
BIN_NAME = chipit
# Compiler used
CXX = ccache g++
# Extension of source files used in the project
SRC_EXT = cpp
# Path to the source directory, relative to the makefile
SRC_PATH = src
# General compiler flags
#COMPILE_FLAGS = -Wall -Wextra -std=c++14 -Wno-unused-variable -Wno-unused-parameter -Wno-unused-function
COMPILE_FLAGS = -Wall -Wextra -std=c++14 -Wno-unused-parameter
# Additional release-specific flags
RCOMPILE_FLAGS = -D NDEBUG -O3
# Additional debug-specific flags
DCOMPILE_FLAGS = -g -ggdb -D DEBUG_ON -D DEVELOPMENT_MODE
# Add additional include paths
INCLUDES = -I $(SRC_PATH)/ -I $(SRC_PATH)/../include -I $(SRC_PATH)/../lib
# General linker settings
#LINK_FLAGS = -lboost_program_options -lboost_random -lm -ltcodxx -lSDL -lSDL2_mixer `pkg-config --libs libconfig++` -Llib -Wl,-rpath=lib
#LINK_FLAGS = -ljsoncpp -lboost_program_options -lboost_random -lboost_serialization -lm -ltcodxx -Llib -Wl,-rpath=lib `sdl2-config --cflags --libs`
#LINK_FLAGS = -lboost_random -Llib -Wl,-rpath=lib `sdl2-config --cflags --libs`
#LINK_FLAGS = -Llib -Wl,-rpath=lib -lsfml-graphics -lsfml-window -lsfml-system
LINK_FLAGS = -Llib -Wl,-rpath=lib -lfmt -lsfml-graphics -lsfml-window -lsfml-system
# Additional release-specific linker settings
RLINK_FLAGS = 
# Additional debug-specific linker settings
DLINK_FLAGS = 
# Destination directory, like a jail or mounted system
DESTDIR = /
# Install path (bin/ is appended automatically)
INSTALL_PREFIX = usr/local
#### END PROJECT SETTINGS ####

# Generally should not need to edit below this line

# Verbose option, to output compile and link commands
export V = true
export CMD_PREFIX = @
ifeq ($(V),true)
	CMD_PREFIX = 
endif

# Combine compiler and linker flags
release: export CXXFLAGS := $(CXXFLAGS) $(COMPILE_FLAGS) $(RCOMPILE_FLAGS)
release: export LD_FLAGS := $(LD_FLAGS) $(LINK_FLAGS) $(RLINK_FLAGS)
debug: export CXXFLAGS := $(CXXFLAGS) $(COMPILE_FLAGS) $(DCOMPILE_FLAGS)
debug: export LD_FLAGS := $(LD_FALGS) $(LINK_FLAGS) $(DLINK_FLAGS)

# Build and output paths
release: export BUILD_PATH := build/release
release: export BIN_PATH := bin/release
debug: export BUILD_PATH := build/debug
debug: export BIN_PATH := bin/debug
install: export BIN_PATH := bin/release

# Find all source files in the source directory
SOURCES = $(shell find $(SRC_PATH)/ -name '*.$(SRC_EXT)')
# Set the object file names, with the source directory stripped
# from the path, and the build path prepended in its place
OBJECTS = $(SOURCES:$(SRC_PATH)/%.$(SRC_EXT)=$(BUILD_PATH)/%.o)
# Set the dependency files that will be used to add header dependencies
DEPS = $(OBJECTS:.o=.d)

# Macros for timing compilation
TIME_FILE = $(dir $@).$(notdir $@)_time
START_TIME = date '+%s' > $(TIME_FILE)
END_TIME = read st < $(TIME_FILE) ; \
	$(RM) $(TIME_FILE) ; \
	st=$$((`date '+%s'` - $$st - 86400)) ; \
	echo `date -u -d @$$st '+%H:%M:%S'` 

# Version macros
# Comment/remove this section to remove versioning
VERSION = $(shell git describe --tags --long --dirty --always | \
	sed 's/v\([0-9]*\)\.\([0-9]*\)-\?.*-\([0-9]*\)-\(.*\)/\1 \2 \3 \4/g')
VERSION_MAJOR = $(word 1, $(VERSION))
VERSION_MINOR = $(word 2, $(VERSION))
VERSION_REVISION = $(word 3, $(VERSION))
VERSION_HASH = $(word 4, $(VERSION))
VERSION_STRING = \
	"$(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_REVISION).$(VERSION_HASH)"
#override CXXFLAGS := $(CXXFLAGS) -D VERSION_MAJOR=$(VERSION_MAJOR) \
#	-D VERSION_MINOR=$(VERSION_MINOR) \
#	-D VERSION_REVISION=$(VERSION_REVISION) \
#	-D VERSION_HASH=\"$(VERSION_HASH)\" \
#	-D VERSION_STRING=\"$(VERSION_STRING)\"
$(shell echo "#ifndef VERSION_H" > src/version.h; \
echo "#define VERSION_H" >> src/version.h; \
echo "#define VERSION_MAJOR $(VERSION_MAJOR)" >> src/version.h; \
echo "#define VERSION_MINOR $(VERSION_MINOR)" >> src/version.h; \
echo "#define VERSION_REVISION $(VERSION_REVISION)" >> src/version.h; \
echo "#define VERSION_HASH \"$(VERSION_HASH)\"" >> src/version.h; \
echo "#define VERSION_STRING \"$(VERSION_STRING)\"" >> src/version.h; \
echo "#endif" >> src/version.h;)

# Standard, non-optimized release build
.PHONY: release
release: dirs
	@echo "Beginning release build v$(VERSION_STRING)"
	#@$(START_TIME)
	@$(MAKE) all --no-print-directory
	#@echo -n "Total build time: "
	#@$(END_TIME)

# Debug build for gdb debugging
.PHONY: debug
debug: dirs
# @echo "Beginning debug build v$(VERSION_STRING)"
# @$(START_TIME)
	@$(MAKE) all --no-print-directory
# @echo -n "Total build time: "
# @$(END_TIME)

# Create the directories used in the build
.PHONY: dirs
dirs:
	@echo "Creating directories"
	@mkdir -p $(dir $(OBJECTS))
	@mkdir -p $(BIN_PATH)

# Installs to the set path
.PHONY: install
install:
	@echo "Installing to $(DESTDIR)$(INSTALL_PREFIX)/bin"
	@install -m 0755 $(BIN_PATH)/$(BIN_NAME) $(DESTDIR)$(INSTALL_PREFIX)/bin

# Removes all build files
.PHONY: clean
clean:
	@echo "Deleting $(BIN_NAME) symlink"
	@$(RM) $(BIN_NAME)
	@echo "Deleting directories"
	@$(RM) -r build
	@$(RM) -r bin

# Main rule, checks the executable and symlinks to the output
all: $(BIN_PATH)/$(BIN_NAME)
#	@echo "Making symlink: $(BIN_NAME) -> $<"
	@$(RM) $(BIN_NAME)
	@ln -s $(BIN_PATH)/$(BIN_NAME) $(BIN_NAME)

# Link the executable
$(BIN_PATH)/$(BIN_NAME): $(OBJECTS)
#	@echo "Linking: $@"
#	@$(START_TIME)
	$(CMD_PREFIX)$(CXX) $(OBJECTS) $(LD_FLAGS) -o $@
#	@echo -en "\t Link time: "
#	@$(END_TIME)

# Add dependency files, if they exist
-include $(DEPS)

# Source file rules
# After the first compilation they will be joined with the rules from the
# dependency files to provide header dependencies
$(BUILD_PATH)/%.o: $(SRC_PATH)/%.$(SRC_EXT)
#  @echo "Compiling: $< -> $@"
#  @$(START_TIME)
	$(CMD_PREFIX)$(CXX) $(CXXFLAGS) $(INCLUDES) -MP -MMD -c $< -o $@
#  @echo -en "\t Compile time: "
#  @$(END_TIME)

