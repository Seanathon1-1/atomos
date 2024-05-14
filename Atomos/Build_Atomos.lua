project "Atomos"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")

    files {
        "src/**.cpp",
        "src/**.hpp",
        "Atomos.hpp",
	    "Build_Atomos.lua"
    }

    --Include directories
    includedirs {
        "./lib/glfw-3.2.1/include",
        "./lib/glew-2.0.0/include",
	    "./lib/ImGui",
        "./lib/glm",
        "src"
    }
   
    
    links {
        "opengl32",
        "glew32s",
        "glfw3",
        "ImGui"
    }    

    defines { "GLEW_STATIC" }

    filter "configurations:Debug"
        defines { "DEBUG" }
        runtime "Debug"
        symbols "On"

    filter "configurations:Release"
        defines { "RELEASE" }
        runtime "Release"
        optimize "On"
        symbols "On"

    filter { }

    print ("test")


    --for 32 bit use these library paths
    filter "architecture:x86"
        libdirs { 
            "./lib/glfw-3.2.1/win32/lib",
            "./lib/glew-2.0.0/win32/lib"
        }
    --for x64 use these
    filter "architecture:x64"
        libdirs { 
            "./lib/glfw-3.2.1/x64/lib",
            "./lib/glew-2.0.0/x64/lib"
        }
    filter { }

    filter { "system:windows", "action:gmake2" }
        buildoptions { "-M" }
    filter { }