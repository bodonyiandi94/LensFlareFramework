local M = {}

function M.library_definition(env) 
    return
    {
        name = "lapack",
        libs = { ["configurations:*"] = "cbia.lib.lapack.dyn.rel.x64.12" }
    }
end

return M