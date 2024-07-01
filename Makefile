
VK_INCLUDE_DIR = C:\VulkanSDK\1.3.283.0\Include
VK_LIB_DIR = C:\VulkanSDK\1.3.283.0\Lib
VK_LIBS = vulkan-1.lib

COMPILE_FLAGS = /c /O2 /Z7 /UUNICODE /DNOMINMAX /I. /I$(VK_INCLUDE_DIR) /DSPIRV_REFLECT_DISABLE_CPP_BINDINGS

CXX_COMPILE_FLAGS = $(COMPILE_FLAGS) /std:c++20
CXX_COMPILER = cl

C_COMPILE_FLAGS = $(COMPILE_FLAGS) /std:c17
C_COMPILER = cl

CXX_LINK_FLAGS = /DEBUG /LIBPATH:$(VK_LIB_DIR) $(VK_LIBS) user32.lib
CXX_LINKER = link

SOURCE_FILES = main.cxx renderer.cxx material.cxx obj_reader.cxx spirv_reflect.c
COMPILED_OBJECTS = $(SOURCE_FILES:.cxx=.obj) $(SOURCE_FILES:.c=.obj)

TARGET = prog.exe

all : $(TARGET)

$(TARGET) : $(COMPILED_OBJECTS)
	$(CXX_LINKER) $(CXX_LINK_FLAGS) $(COMPILED_OBJECTS) /out:$(TARGET)

main.obj : main.cxx
	$(CXX_COMPILER) $(CXX_COMPILE_FLAGS) main.cxx /out:main.obj
material.obj : material.cxx
	$(CXX_COMPILER) $(CXX_COMPILE_FLAGS) material.cxx /out:material.obj
renderer.obj : renderer.cxx
	$(CXX_COMPILER) $(CXX_COMPILE_FLAGS) renderer.cxx /out:renderer.obj
obj_reader.obj : obj_reader.cxx
	$(CXX_COMPILER) $(CXX_COMPILE_FLAGS) obj_reader.cxx /out:obj_reader.obj
spirv_reflect.obj : spirv_reflect.c
	$(C_COMPILER) $(C_COMPILE_FLAGS) spirv_reflect.c /out:spirv_reflect.obj

clean :
	del $(COMPILED_OBJECTS) $(TARGET)
