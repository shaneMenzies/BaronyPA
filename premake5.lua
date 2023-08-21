workspace "baronyPA"
    configurations { "Debug", "Release" }
	
	filter "action:vs*"
		buildoptions { "/Zc:__cplusplus" }
		platforms { "Win64" }
		
	filter "action:gmake*"
		platforms { "Linux64", "Mac64" }
	
	filter "Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "Release"
        defines { "NDEBUG" }
        optimize "Speed"
		
	filter "platforms:*64"
		architecture "x86_64"
		
	filter "platforms:Win64" 
		system "windows"
		
	filter "platforms:Linux64"
		system "linux"
		defines { "OS_UNIX" }
		
	filter "platforms:Mac64"
		system "macosx"
		defines { "OS_UNIX" }

project "baronyPA"
    kind "SharedLib"
    language "C++"
	cppdialect "C++17"
    targetdir "bin/%{cfg.buildcfg}"
	includedirs { "include" }
	defines { "_CRT_SECURE_NO_WARNINGS", "_SCL_SECURE_NO_WARNINGS", "ASIO_STANDALONE" }

    files { "**.cpp" }