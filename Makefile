
INCLUDE_DIRS = C:\VulkanSDK\1.3.268.0\Include

main.obj : main.cxx
	cl /c main.cxx /Z7 -IC:\VulkanSDK\1.3.268.0\Include -I. /std:c++20 /UUNICODE /DNOMINMAX /out:main.obj
renderer.obj : renderer.cxx
	cl /c renderer.cxx /Z7 -IC:\VulkanSDK\1.3.268.0\Include -I. /std:c++20 /DNOMINMAX /UUNICODE /out:renderer.obj
material.obj : material.cxx
	cl /c material.cxx /Z7 -I. /std:c++20 /DNOMINMAX /UUNICODE /out:material.obj

prog.exe : main.obj renderer.obj material.obj
	link /DEBUG main.obj renderer.obj material.obj user32.lib vulkan-1.lib /LIBPATH:C:\VulkanSDK\1.3.268.0\Lib /out:prog.exe
