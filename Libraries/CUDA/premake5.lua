local M = {}

function M.library_configure(env) 
    newoption{
        trigger     = "cuda_root",
        value       = "PATH",
        description = "Root folder of the Cuda installation",
        default     = "c:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v10.1/"
    }
end

function M.library_definition(env) 
    root_folder = env["options"]["cuda_root"]
    return
    {
        name = "CUDA",
        paths =
        {            
            ["root"] = root_folder,
            ["includes"] = path.join(root_folder, 'include'),
            ["libraries"] = path.join(root_folder, 'lib/x64'),
            ["binaries"] = path.join(root_folder, 'bin')
        },
        libs = 
        {
            ["configurations:*"] = { "nvrtc", "cufftw", "cufft" }
        },
        dlls = 
        {
            ["configurations:*"] = { "nvrtc64_101_0", "nvrtc-builtins64_101", "cufftw64_10", "cufft64_10" }
        },
        optional = true
    }
end

return M