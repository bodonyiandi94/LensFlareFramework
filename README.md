# Reference Implementation for *Real-time ray transfer for lens flare rendering using sparse polynomials* and *Efficient tile-based rendering of lens flare ghosts*

This repository contains our reference implementation for the paper *Efficient tile-based rendering of lens flare ghosts* and our manuscript *Real-time ray transfer for lens flare rendering using sparse polynomials* (currently under review).

## Dependencies

The provided implementation requires the following external software:

- [Microsoft Visual Studio](https://visualstudio.microsoft.com/vs/)
    - Tested with v16.11.30.
    - [SmartCommandlineArguments](https://marketplace.visualstudio.com/items?itemName=MBulli.SmartCommandlineArguments) is recommended (`.json` file is auto-generated by the build script).

The source code is uploaded with the rest of the necessary binaries pre-packaged for building with VS 2019.

## Hardware Requirements

Our reference implementation makes heavy use of compute shaders, and thus a DirectX 11 / OpenGL 4.3 compatible video card is required.

For reference, all of our tests and performance measurements were performed on the following system configuration:

- **CPU**: AMD Ryzen 7 1700X
- **GPU**: NVIDIA TITAN Xp
- **Memory**: 32 GBytes

## Running the Sample

### Generating build files

Our implementation relies on [Premake5](https://github.com/premake/premake-core/releases) to generate the necessary build files. Premake5 is included in the archive; to invoke it, use the following command in the project's main folder:

```
premake5 vs2019
```

After Premake is finished, the generated build files can be found in the [Build](Build) folder. 

### Building with Visual Studio

The solution can be opened in Visual Studio and built simply by selecting the desired build configuration (we recommend using a `Release` build for optimal performance). No additional steps are required.

### Building with MSBuild

Alternatively, the project can also be built using MSBuild. 

1. Open the [VS Developer Command Prompt](https://docs.microsoft.com/en-us/dotnet/framework/tools/developer-command-prompt-for-vs).
2. Navigate to the [Build](Build) folder.
3. Build the project using `msbuild \p:Configuration=Release`.

### Running the Program

In Visual Studio, the program can be started simply using the `Start Without Debugging` option. Alternatively, the compiled executable can be manually launched from the [Build](Build) folder. Note, however, that the required `.dll` files are not automatically installed into the build folder, and thus, must be manually copied over by the user from the individual [Libraries](Libraries) subfolders.

Our implementation uses sensible defaults for the rendering arguments. Overriding these can be done in the following ways:

1. If using `SmartCommandLineArguments`, the set of active arguments can be set via the extension's window (accessible via `View/Other Windows`).
2. In the absence of the aforementioned extension, the arguments can be set manually via the project settings window, located under the `Debugging` category.

The list of command line arguments can be observed in the [Source/Core/Config.cpp](Source/Core/Config.cpp) file.

## Code organization

The relevant C++ source code is available in the [Source/Scene/Components/LensFlare](Source/Scene/Components/LensFlare) folder, with the corresponding shaders residing in [Assets/Shaders/OpenGL/LensFlare](Assets/Shaders/OpenGL/LensFlare).

### Camera description

The `PhysicalCamera` class (stored in [Source/Scene/Components/LensFlare/PhysicalCamera](Source/Scene/Components/LensFlare/PhysicalCamera.h)) is responsible for describing the physical structure of the simulated camera. The optical system descriptions are stored on disk, located in the [Assets/Lenses](Assets/Lenses/) folder.

### Polynomial fitting

The code responsible for polynomial fitting resides in [Source/Scene/Components/LensFlare/TiledLensFlare.cpp](Source/Scene/Components/LensFlare/TiledLensFlare.cpp). The relevant code can be found in the following namespaces:

- `PolynomialsCommon`: Provides common functionality that is used by all polynomial optics-based lens flare implementations.
- `PolynomialsFull`: Implements the naive polynomial fitting approach where a single polynomial system is fit to the entire input domain of the optical system.
- `PolynomialsPartial`: Implements our proposed polynomial fitting approach where partial fitting regions are defined over the input domain and used during fitting.

In shaders, the ray-tracing code is found in the [Assets/Shaders/OpenGL/LensFlare/RayTraceLensFlare/TraceRays/Methods](Assets/Shaders/OpenGL/LensFlare/RayTraceLensFlare/TraceRays/Methods/) folder. The following files are relevant:

- [polynomial_common.glsl](Assets/Shaders/OpenGL/LensFlare/RayTraceLensFlare/TraceRays/Methods/polynomial_common.glsl): Contains shared code that is reused by both polynomial approaches, such as GPU buffers, monomial and polynomial evaluation, etc.
- [polynomial_full_fit.glsl](Assets/Shaders/OpenGL/LensFlare/RayTraceLensFlare/TraceRays/Methods/polynomial_full_fit.glsl): Contains code specific to polynomial systems that describe the full input domain of the optical system.
- [polynomial_partial_fit.glsl](Assets/Shaders/OpenGL/LensFlare/RayTraceLensFlare/TraceRays/Methods/polynomial_partial_fit.glsl): Contains code specific to our proposed approach, whereby polynomials are fit to partial input regions.

### Rendering (CPU)

The rendering code also resides in [Source/Scene/Components/LensFlare/TiledLensFlare.cpp](Source/Scene/Components/LensFlare/TiledLensFlare.cpp). Generally speaking, the most important functions of these classes are the following:
- `initObject`: Responsible for creating the necessary GPU buffers and loading assets.
- `renderObject`: Main entry point for rendering the object.
- `generateGui`: Generates the user interface for modifying the exposed parameters. Each class relies on on-demand data recomputation, and thus, the relevant processes are initiated by these functions. 
- `demoSetup`: Instantiates the object and configures it for the demo scene.

Because our implementation uses on-demand data computation, changing the values while running the sample can result in a full eye reconstruction in the worst-case scenario. Our typical workflow is to set all the parameters in the `demoSetup` function, then build and run the sample with the updated parameters, as we only need to do a single pre-computation step with such an approach.

### Rendering (shaders)

The important shaders are in the [Assets/Shaders/OpenGL/LensFlare/RayTraceLensFlare](Assets/Shaders/OpenGL/LensFlare/RayTraceLensFlare) folder. The shaders are organized as follows:

- [TraceRays](Assets/Shaders/OpenGL/LensFlare/RayTraceLensFlare/TraceRays/): Contains the implementations of the various approaches to tracing a ray through the input optical system (analytical and polynomial).
- [PrecomputeGhosts](Assets/Shaders/OpenGL/LensFlare/RayTraceLensFlare/PrecomputeGhosts/): Implements ray tracing using analytical formulas and stores the data in GPU buffers. Used for ghost bounding, fitting data generation, etc.
- [RenderGhosts](Assets/Shaders/OpenGL/LensFlare/RayTraceLensFlare/RenderGhosts/): Implements the original approach of Hullin et al., whereby a coarse ray grid is traced per ghost, which is then triangulated and rendered on the fly.
- [RenderGhostsTiled](Assets/Shaders/OpenGL/LensFlare/RayTraceLensFlare/RenderGhostsTiled/): Implements our proposed, tile-based ghost rasterization approach, where the triangles of the coarse ray grid are collected into tiles and rendered in one step.

## Related publications & citations

If you find the framework or our algorithms useful, we kindly ask you to cite the relevant papers as follows:

- [Efficient tile-based rendering of lens flare ghosts](https://doi.org/10.1016/j.cag.2023.07.019)

```
@article{bodonyi2023efficient,
  title={Efficient tile-based rendering of lens flare ghosts},
  author={Bodonyi, Andrea and Kunkli, Roland},
  journal={Computers \& Graphics},
  volume={115},
  pages={472--483},
  year={2023},
  publisher={Elsevier},
  doi = {https://doi.org/10.1016/j.cag.2023.07.019},
}

```

- Real-time ray transfer for lens flare rendering using sparse polynomials (under review)

```
@unpublished{bodonyi2024real,
  title={Real-time ray transfer for lens flare rendering using sparse polynomials},
  author={Bodonyi, Andrea and Csoba, István and Kunkli, Roland},
  year={2024},
  note={Manuscript under review}
}
```
