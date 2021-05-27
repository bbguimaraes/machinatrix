local gmake2 = require "gmake2"

local p = premake
local project = p.project

local format_files = {}
local tidy_files = {}

local function format_on_project(prj)
    for _, f in ipairs(prj._.files) do
        format_files[path.getrelative(_WORKING_DIR, f.abspath)] = true
    end
end

local function format_execute()
    local l = table.keys(format_files)
    table.sort(l)
    for _, f in ipairs(l) do
        local cmd = string.format("clang-format -i %s", p.quoted(f))
        verbosef("%s", cmd)
        os.execute(cmd)
    end
end

local function tidy_on_project(prj)
    local cfg = project.getconfig(
        prj, _OPTIONS["tidy-configuration"], _OPTIONS["tidy-platform"])
    local dir = prj.basedir
    local reldir = path.getrelative(_WORKING_DIR, prj.basedir)
    for _, f in ipairs(prj._.files) do
        if path.getextension(f.abspath) ~= ".c" then
            goto continue
        end
        tidy_files[path.getrelative(dir, f.abspath)] = {
            file = f,
            cfg = cfg,
            dir = reldir,
        }
        ::continue::
    end
end

local function tidy_execute()
    for filename, t in pairs(tidy_files) do
        local cfg = t.cfg
        local toolset = gmake2.getToolSet(cfg)
        local flags = table.concat(
            table.join(
                toolset.getdefines(cfg.defines, cfg),
                toolset.getincludedirs(
                    cfg, cfg.includedirs, cfg.sysincludedirs),
                toolset.getforceincludes(cfg),
                toolset.getcflags(cfg),
                cfg.buildoptions),
            " ")
        local cmd = string.format(
            "cd %s && clang-tidy %s -- %s",
            p.quoted(t.dir), p.quoted(filename), flags)
        verbosef("%s\n%s", path.getrelative(_WORKING_DIR, t.file.abspath), cmd)
        os.execute(cmd)
    end
end

newoption {
    trigger = "tidy-configuration",
    description =
        "configuration whose flags are used to build the"
        .. " clang-tidy command line",
}

newoption {
    trigger = "tidy-platform",
    description =
        "platform whose flags are used to build the"
        .. " clang-tidy command line",
}

newaction {
    trigger = "format",
    description = "apply clang-format to all source files",
    onProject = format_on_project,
    execute = format_execute,
}

newaction {
    trigger = "tidy",
    description = "apply clang-tidy to all source files",
    onProject = tidy_on_project,
    execute = tidy_execute,
}
