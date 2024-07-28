local M = {}

function M.library_definition(env) 
    return
    {
        name = "Assimp",
        libs = 
        { 
            ["configurations:*Debug*"] = "assimp-vc142-mtd",
            ["configurations:*Development*"] = "assimp-vc142-mt",
            ["configurations:*Release*"] = "assimp-vc142-mt",
        } 
    }
end

return M