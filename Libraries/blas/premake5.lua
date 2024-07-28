local M = {}

function M.library_definition(env) 
    return
    {
        name = "blas",
        libs = { ["configurations:*"] = "cbia.lib.blas.dyn.rel.x64.12" }
    }
end

return M