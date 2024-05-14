-- premake5.lua


workspace "Atomos" -- Name of sln file
    configurations { "Debug", "Release" }
    platforms { "Win32", "x64" }
    toolset "msc"

    outputdir = ("bin/%{cfg.buildcfg}/%{cfg.architecture}")
    
    cppdialect "C++20"

    --Set architecture
    filter { "platforms:Win32" }
        system "Windows"
        architecture "x86"
    
    filter { "platforms:x64" }
        system "Windows"
        architecture "x64"
    filter { }

    filter { "system:windows", "action:gmake2" }
        buildoptions { "--std=c++2a" }
    filter { }

    group "Dependencies"
        include "Atomos/lib/Imgui/Build_ImGui.lua"

    group "Core"
        include "Atomos/Build_Atomos.lua"


    group "Demo"
        include "Balls/Build_Balls.lua"
