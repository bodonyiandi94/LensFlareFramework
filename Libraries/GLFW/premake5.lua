local M = {}

function M.library_definition(env) 
    return
    {
        name = "GLFW",
        libs = { ["configurations:*"] = "glfw3"}
    }
end

return M