local M = {}

function M.library_definition(env) 
    return
    {
        name = "GLEW",
        libs = { ["configurations:*"] = "glew32s" }
    }
end

return M