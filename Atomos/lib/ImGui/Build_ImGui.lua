project "ImGui"
    kind "StaticLib"
    language "C++"
    staticruntime "off"


    targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")

    includedirs { 
        ".",
        "../glfw-3.2.1/include",
        "../glew-2.0.0/include"
    }
    
    files { 
        "./*.h",
        "./*.cpp",
        "backends/*_glfw.*",
        "backends/*_opengl3*",
        "Build_ImGui.lua"
    } 