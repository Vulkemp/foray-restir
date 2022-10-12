# Rendering intels sponza scene with the rt rpf framework

## Prerequisites

- **Vulkan SDK 1.2+** Follow install instructions on [https://vulkan.lunarg.com/sdk/home](https://vulkan.lunarg.com/sdk/home)
- **SDL2 Dev Libraries**
    - Linux: Install package `libsdl2-dev`
    - Windows: `foray_rt_rpf` repository contains headers and windows libraries of SDL 2.0.20. No further setup is required.

## Cloning

Requires a submodule (`--recursive` clone option or run `submodule init` after cloning)
```
git clone --recursive https://github.com/Vulkemp/foray_rt_rpf_sponza_sample
```

## Download Intels scene sponza sample
https://www.intel.com/content/www/us/en/developer/topic-technology/graphics-research/samples.html

You can manually download the files or run the download_sponza.py. Note that the download_sponza.py requires modules to be installed.

The folder structure of the extracted sponza files should be according to this schema:

```
foray_rt_rpf_sponza_sample
│   README.md 
|   download_sponza.py
│   ...
└───sponza_model
│   └───Main
│   └───PKG_A_Curtains
│   └───PKG_B_Ivy
│   └───PKG_C_Trees
│   └───PKG_D_Candles
```

## Configure and Build

That's all set up, now ready to be configured and built.