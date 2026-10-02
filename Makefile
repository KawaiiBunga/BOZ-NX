TARGET := codboz_native
SPLIT_DIAGNOSTICS ?= 0
PORT_NPDM_PROGRAM_ID := 0x01000000000010B0
PORT_NPDM_MAIN_STACK := 0x800000
PORT_CFLAGS := -ffp-contract=off -Ithird_party/imgui
# This game needs the existing Marmalade SDK, not Android's Java runtime.
# Reuse android32's proven relocation/mapping support and launcher only.
RT_KEEP := crt0_reloc.c nx32_virtmem.c selfproc.c code_flush.c
RT_EXCLUDE := $(filter-out $(RT_KEEP),$(notdir $(wildcard runtime/source/*.c runtime/source/*.S)))
include runtime/runtime.mk
comma := ,
UNUSED_WRAPS := -Wl$(comma)--wrap=svcSetThreadCoreMask -Wl$(comma)--wrap=_svfprintf_r -Wl$(comma)--wrap=_vfprintf_r -Wl$(comma)--wrap=nouveau_bo_new
LDFLAGS := $(filter-out $(UNUSED_WRAPS),$(LDFLAGS))
LIBS := -L$(PORTLIBS)/lib -lGLESv1_CM $(LIBS)
CPPFILES := $(wildcard source/*.cpp third_party/imgui/*.cpp)
CPPOBJS := $(addprefix $(BUILD)/,$(notdir $(CPPFILES:.cpp=.o)))
OFILES += $(CPPOBJS)
CXXFLAGS := $(filter-out -Werror=implicit-function-declaration -Werror=implicit-int -Werror=int-conversion -Werror=incompatible-pointer-types,$(CFLAGS)) -fno-rtti -fno-exceptions -DIMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS
vpath %.cpp source third_party/imgui
$(BUILD)/%.o: %.cpp $(RENDERER_STAMP) | $(BUILD) $(BUILD)/dcr_build.h
	@echo $(notdir $<)
	@$(CXX) -MMD -MP $(CXXFLAGS) -c $< -o $@
$(TARGET).elf: $(CPPOBJS)
$(BUILD)/main.o: source/boz_build_id.h
$(BUILD)/menu.o: source/boz_build_id.h
source/boz_build_id.h: FORCE
	@python3 tools/build_id.py --split-diagnostics $(SPLIT_DIAGNOSTICS)
