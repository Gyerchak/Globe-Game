CXX      := g++
CXXFLAGS := -std=c++20 -O3 -Iinclude -Isrc -I/usr/include/vulkan -I/usr/include/glm
LDFLAGS  := -lgdal -lvulkan -lglfw

TARGET   := Globe
SRCDIR   := src
OBJDIR   := obj

# Collect all .cpp sources recursively
SRCS     := $(shell find $(SRCDIR) -name '*.cpp')
OBJS     := $(patsubst $(SRCDIR)/%.cpp,$(OBJDIR)/%.o,$(SRCS))

# Shader files
SHADER_SRCDIR := src/shaders
SHADER_OUTDIR := shaders
VERT_SRC := $(wildcard $(SHADER_SRCDIR)/*.vert)
FRAG_SRC := $(wildcard $(SHADER_SRCDIR)/*.frag)
SHADER_OUT := $(patsubst $(SHADER_SRCDIR)/%.vert,$(SHADER_OUTDIR)/%.spv,$(VERT_SRC)) \
			 $(patsubst $(SHADER_SRCDIR)/%.frag,$(SHADER_OUTDIR)/%.spv,$(FRAG_SRC))

# ✅ Now shaders are part of the default build
all: shaders $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp $(SRCDIR)/GlobeApp.h | $(OBJDIR)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

# Compile all shaders
shaders: $(SHADER_OUT)

$(SHADER_OUTDIR)/%.spv: $(SHADER_SRCDIR)/%.vert
	glslc $< -o $@

$(SHADER_OUTDIR)/%.spv: $(SHADER_SRCDIR)/%.frag
	glslc $< -o $@

.PHONY: clean shaders
clean:
	rm -rf $(OBJDIR) $(TARGET) shaders/*.spv

.DEFAULT_GOAL := all