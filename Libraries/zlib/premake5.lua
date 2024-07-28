local M = {}

function M.library_definition(env) 
    return
    {
        name = "zlib",
        libs = 
        {
            ["configurations:*Debug*"] = "zlibd",
            ["configurations:*Development*"] = "zlib",
            ["configurations:*Release*"] = "zlib"
        },
        dlls = 
        {
            ["configurations:*Debug*"] = "zlibd.dll",
            ["configurations:*Development*"] = "zlib.dll",
            ["configurations:*Release*"] = "zlib.dll"
        } 
    }
end

return M