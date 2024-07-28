local M = {}

function M.library_definition(env) 
    return
    {
        name = "gsl",
        libs = { ["configurations:*"] = { "cblas", "gsl" } } 
    }
end

return M