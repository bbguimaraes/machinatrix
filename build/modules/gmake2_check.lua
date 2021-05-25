local gmake2 = require "gmake2"

local p = premake

p.override(gmake2, "generate_workspace", function(base, wks)
    base(wks)
    p.pop()
    p.w()
    p.w(".PHONY: check")
    p.w("check: | all")
    for prj in p.workspace.eachproject(wks) do
        if not table.contains(prj.tags, "test") then goto continue end
        local prjpath = p.filename(prj, gmake2.getmakefilename(prj, true))
        p.x(
            "\t@${MAKE} --no-print-directory -C %s -f %s check",
            path.getdirectory(path.getrelative(wks.location, prjpath)),
            path.getname(prjpath))
        ::continue::
    end
end)

p.override(gmake2.cpp, "generate", function(base, prj)
    base(prj)
    p.w()
    p.w(".PHONY: check")
    p.w("check:")
    if table.contains(prj.tags, "test") then
        p.w('\t@echo "==== Executing test %s ===="', prj.name)
        p.w("\t@$(TARGET)")
    end
end)
