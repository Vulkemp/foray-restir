# ReSTIR in foray
This repository hosts the code that was developed alongside with the masters thesis "Comparison and Evaluation Of Modern Sampling Techniques For Realtime Ray Tracing".
The application was developed on top of the rapid prototyping framework "foray".
The ReSTIR shadercode is based on https://github.com/lukedan/ReSTIR-Vulkan

Working as of the 16th of October 2023 with Vulkan 1.3.261.1.

## Prerequisites

- **Vulkan 1.3.261.1** (Other versions not tested) Follow install instructions on [https://vulkan.lunarg.com/sdk/home](https://vulkan.lunarg.com/sdk/home)

# Setup
* Clone the repository with its submodules.
```
git clone --recursive https://github.com/Vulkemp/foray_restir.git
```

Clones the repository and the foray submodule.

# Available scenes

The repository contains three main scenes: emissive spheres, pillar room and bistro exterior that were tested. A scene can be chosen in the `restir_app.cpp`
file in the `void RestirProject::loadScene()` method. Simply comment in the desired scene path.

## Bistro Exterior
Due to size restrictions before using the bistro exterior scene, the gltf binary must be unzipped. Inside the path `data/scenes/bistro_exterior` the `buffer.zip` file must be unzipped, so the `buffer.bin` is available in the directory.
