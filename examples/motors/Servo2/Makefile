#   Copyright 2016-2021 Jean-Francois Poilpret
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

# Specific to FastArduino examples: we use the current directory name as
# the target name
# That allows using the same Makefile for all examples
THISPATH:=$(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Set necessary variables for generic makefile
# Name of target (binary and derivatives)
TARGET:=$(lastword $(subst /, ,$(THISPATH)))
# Where to search for source files (.cpp)
SOURCE_ROOT:=.
# Where FastArduino project is located (used to find library and includes)
FASTARDUINO_ROOT=../../..
# Additional paths containing includes (usually empty)
ADDITIONAL_INCLUDES:=
# Additional paths containing libraries other than fastarduino (usually empty)
ADDITIONAL_LIBS:=

# include generic makefile for apps
include $(FASTARDUINO_ROOT)/make/Makefile-app.mk

