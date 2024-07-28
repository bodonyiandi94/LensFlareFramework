local M = {}

function M.library_definition(env) 
    return
    {
        name = "OpenGL",
        libs = { ["configurations:*"] =  "opengl32"} 
    }
end

return M