local M = {}

function M.library_definition(env) 
    return
    {
        name = "WinAPI",
        libs = { ["configurations:*"] = { "vfw32.lib", "PowrProf.lib", "Winmm.lib" } }
    }
end

return M