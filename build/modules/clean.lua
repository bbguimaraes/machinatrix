if _ACTION == "clean" then
    for _, x in ipairs{"docs/html", "docs/latex"} do
        x = path.join(_MAIN_SCRIPT_DIR, x)
        if not os.isdir(x) then goto continue end
        verbosef("removing %s", x)
        local ret, err = os.rmdir(x)
        if not ret then error(err) end
        ::continue::
    end
end

