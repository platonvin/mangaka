.ONESHELL:

#setting up include and lib directories for dependencies
I = -Ilum-al/src
L = -Llum-al/lib

OTHER_DIRS := $(filter-out vcpkg_installed/vcpkg, $(wildcard vcpkg_installed/*))
INCLUDE_LIST := $(addsuffix /include, $(OTHER_DIRS))
INCLUDE_LIST += $(addsuffix /include/vma, $(OTHER_DIRS))
INCLUDE_LIST += $(addsuffix /include/volk, $(OTHER_DIRS))
INCLUDE_LIST := $(addprefix -I, $(INCLUDE_LIST))
LIB_LIST := $(addsuffix /lib, $(OTHER_DIRS))
LIB_LIST := $(addprefix -L, $(LIB_LIST))

I += $(INCLUDE_LIST)
L += $(LIB_LIST)

#spirv things are installed with vcpkg and not set in envieroment, so i find needed tools myself
GLSLC_DIR := $(firstword $(foreach dir, $(OTHER_DIRS), $(wildcard $(dir)/tools/shaderc)))
GLSLC = $(GLSLC_DIR)/glslc

# IT MIGHT RANDOMLY WORK SOMETIMES
ifeq ($(OS),Windows_NT)
	GLSLC := $(subst /,\\,$(GLSLC))
endif

objs := \
	obj/main.o\
	obj/vktf/basisu/transcoder/basisu_transcoder.o\
	obj/vktf/VulkanglTFModel.o\

ifeq ($(OS),Windows_NT)
	REQUIRED_LIBS = -lglfw3 -lgdi32        -lvolk -lzstd
	STATIC_OR_DYNAMIC += -static
else
	REQUIRED_LIBS = -lglfw3 -lpthread -ldl -lvolk -lzstd
endif
	
always_enabled_flags = -fno-exceptions -Wuninitialized -std=c++20
optimize_flags = -Ofast -mmmx -msse -msse2 -msse3 -mssse3 -msse4.1 -msse4.2 -mcx16 -mavx -mpclmul 

#default target
profile = -D_PRINTLINE
# profile = 
all: init mangaka
native: optimize_flags = -Ofast -march=native -s
native: init mangaka

obj/%.o: src/%.cpp
	c++ $(always_enabled_flags) $(I) $(args) $(profile) -MMD -MP -c $< -o $@
DEPS = $(objs:.o=.d)
-include $(DEPS)
obj/vktf/%.o: src/vktf/%.cpp
	c++ $(always_enabled_flags) $(optimize_flags) $(I) $(args) $(profile) -MMD -MP -c $< -o $@
DEPS = $(objs:.o=.d)
-include $(DEPS)
obj/vktf/basisu/transcoder/%.o: src/vktf/basisu/transcoder/%.cpp
	c++ $(always_enabled_flags) $(optimize_flags) $(I) $(args) $(profile) -MMD -MP -c $< -o $@
DEPS = $(objs:.o=.d)
-include $(DEPS)
#TODO dependencies
lum-al/lib/liblumal.a:
	cd lum-al
	make library
	cd ..

mangaka: init build shaders
ifeq ($(OS),Windows_NT)
	.\mangaka
else
	./mangaka
endif

SHADER_SRC_DIR = shaders
SHADER_OUT_DIR = shaders/compiled

COMP_EXT = .comp
VERT_EXT = .vert
FRAG_EXT = .frag
GEOM_EXT = .geom

COMP_SHADERS = $(wildcard $(SHADER_SRC_DIR)/*$(COMP_EXT))
VERT_SHADERS = $(wildcard $(SHADER_SRC_DIR)/*$(VERT_EXT))
FRAG_SHADERS = $(wildcard $(SHADER_SRC_DIR)/*$(FRAG_EXT))
GEOM_SHADERS = $(wildcard $(SHADER_SRC_DIR)/*$(GEOM_EXT))
SHADERS = $(wildcard $(SHADER_SRC_DIR)/*$(GEOM_EXT))

COMP_TARGETS = $(patsubst $(SHADER_SRC_DIR)/%$(COMP_EXT), $(SHADER_OUT_DIR)/%.spv, $(COMP_SHADERS))
VERT_TARGETS = $(patsubst $(SHADER_SRC_DIR)/%$(VERT_EXT), $(SHADER_OUT_DIR)/%Vert.spv, $(VERT_SHADERS))
FRAG_TARGETS = $(patsubst $(SHADER_SRC_DIR)/%$(FRAG_EXT), $(SHADER_OUT_DIR)/%Frag.spv, $(FRAG_SHADERS))
GEOM_TARGETS = $(patsubst $(SHADER_SRC_DIR)/%$(GEOM_EXT), $(SHADER_OUT_DIR)/%Geom.spv, $(GEOM_SHADERS))

ALL_SHADER_TARGETS = $(COMP_TARGETS) $(VERT_TARGETS) $(FRAG_TARGETS) $(GEOM_TARGETS)

$(SHADER_OUT_DIR)/%.spv: $(SHADER_SRC_DIR)/%$(COMP_EXT)
	$(GLSLC) -o $@ $< $(SHADER_FLAGS)
$(SHADER_OUT_DIR)/%Vert.spv: $(SHADER_SRC_DIR)/%$(VERT_EXT)
	$(GLSLC) -o $@ $< $(SHADER_FLAGS)
$(SHADER_OUT_DIR)/%Frag.spv: $(SHADER_SRC_DIR)/%$(FRAG_EXT)
	$(GLSLC) -o $@ $< $(SHADER_FLAGS)
$(SHADER_OUT_DIR)/%Geom.spv: $(SHADER_SRC_DIR)/%$(GEOM_EXT)
	$(GLSLC) -o $@ $< $(SHADER_FLAGS)

shaders: $(ALL_SHADER_TARGETS)

# # c++ $(lib_objs) $(I) $(L) $(REQUIRED_LIBS) $(STATIC_OR_DYNAMIC) -c -o lumal 
# library: $(lib_objs)
# 	ar rvs lib/liblumal.a $(lib_objs)

build: $(objs) lum-al/lib/liblumal.a 
	c++ -o mangaka $(objs) -llumal $(flags) $(I) $(L) $(REQUIRED_LIBS) $(STATIC_OR_DYNAMIC)

init: obj obj/vktf obj/vktf/basisu obj/vktf/basisu/transcoder shaders/compiled
obj:
ifeq ($(OS),Windows_NT)
	mkdir "obj"
else
	mkdir -p obj
endif
obj/vktf:
ifeq ($(OS),Windows_NT)
	mkdir "obj\vktf"
else
	mkdir -p obj/vktf
endif
obj/vktf/basisu:
ifeq ($(OS),Windows_NT)
	mkdir "obj\vktf\basisu"
else
	mkdir -p obj/vktf/basisu
endif
obj/vktf/basisu/transcoder:
ifeq ($(OS),Windows_NT)
	mkdir "obj\vktf\basisu\transcoder"
else
	mkdir -p obj/vktf/basisu/transcoder
endif
shaders/compiled:
ifeq ($(OS),Windows_NT)
	mkdir "shaders\compiled"
else
	mkdir -p shaders/compiled
endif

clean:
ifeq ($(OS),Windows_NT)
	del "obj\*.o"
	del "obj\vktf\*.o"
	del "obj\vktf\basisu\transcoder\*.o"
else
	rm -R obj/*.o
endif