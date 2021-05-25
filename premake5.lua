require "build.modules.clean"
require "build.modules.gmake2_check"
require "build.modules.gmake2_clang_tools"

newaction {
    trigger = "docs",
    description = "build Doxygen documentation",
    execute = function() os.execute("doxygen") end,
}

newoption {
    trigger = "build-dir",
    value = "path",
    default = "build",
    description = "directory where build files will be placed",
}

newoption {
    trigger = "target-dir",
    value = "path",
    default = "%{wks.location}",
    description = "directory under which output files will be placed",
}

workspace "machinatrix"
    location(_OPTIONS["build-dir"])
    configurations { "debug", "release" }
    kind "ConsoleApp"
    language "C"
    cdialect "C11"
    defines { "_POSIX_C_SOURCE=200809L" }
    warnings "Extra"
    targetdir(path.join(_OPTIONS["target-dir"], "bin/%{cfg.buildcfg}"))

    filter "configurations:debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:release"
        defines { "NDEBUG" }
        optimize "On"

    filter "toolset:clang or gcc"
        enablewarnings { "pedantic", "conversion" }

    filter { "toolset:clang or gcc", "tags:test" }
        buildoptions { "-fsanitize=address,undefined", "-fstack-protector" }
        linkoptions { "-fsanitize=address,undefined", "-fstack-protector" }

    include "src"
    include "tests"
