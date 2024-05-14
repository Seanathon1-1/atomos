project "Balls"
    kind "ConsoleApp"
    language "C++"
    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")

    files { 
	    "./src/**.hpp", 
	    "./src/**.cpp",
	    "Build_Balls.lua" 
    } 


    links {
        "Atomos"
    }

    includedirs {
        "src",
        "%{wks.location}/Atomos/src"
    }
    
    --Debug and Release configurations
    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"

    filter { }

    filter { "system:windows", "action:gmake2" }
        buildoptions { "-M" }
    filter { }

