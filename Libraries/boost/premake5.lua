local M = {}

function M.library_definition(env) 
    return
    {
        name = "boost",
        libs = 
        {
            ["configurations:*Debug*"] = "boost_math_tr1-vc141-mt-gd-x64-1_69",
            ["configurations:*Development*"] = "boost_math_tr1-vc141-mt-x64-1_69",
            ["configurations:*Release*"] = "boost_math_tr1-vc141-mt-x64-1_69"
        } 
    }
end

return M