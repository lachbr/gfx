
INCLUDE_DIRS = C:\VulkanSDK\1.3.283.0\Include

main.obj : main.cxx
	cl /c main.cxx -IC:\VulkanSDK\1.3.283.0\Include /std:c++20 /UUNICODE /out:main.obj

prog.exe : main.obj
	link main.obj user32.lib vulkan-1.lib /LIBPATH:C:\VulkanSDK\1.3.283.0\Lib /out:prog.exe
