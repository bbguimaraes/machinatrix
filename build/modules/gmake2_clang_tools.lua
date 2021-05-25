local gmake2 = require "gmake2"

local p = premake

p.override(gmake2, "generate_workspace", function(base, wks)
    base(wks)
    for _, t in ipairs{"format", "tidy"} do
        p.w()
        p.w("%s:", t)
        for prj in p.workspace.eachproject(wks) do
            local prjpath = p.filename(prj, gmake2.getmakefilename(prj, true))
            p.x(
                "\t@${MAKE} --no-print-directory -C %s -f %s %s",
                path.getdirectory(path.getrelative(wks.location, prjpath)),
                path.getname(prjpath), t)
        end
    end
end)

p.override(gmake2.cpp, "generate", function(base, prj)
    base(prj)
    p.w()
    p.w(".PHONY: format")
    p.w("format:")
    for _, f in ipairs(prj._.files) do
        p.x("\tclang-format -i %s", f.relpath)
    end
    p.w()
    p.w(".PHONY: tidy")
    p.w("tidy:")
    for _, f in ipairs(prj._.files) do
        if path.getextension(f.relpath) == ".c" then
            p.x("\tclang-tidy %s -- $(INCLUDES) %s %s", f.relpath, "", "")
        end
    end
end)
