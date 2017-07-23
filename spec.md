# Terminology
**Cloud texture**: Translucent texture mapped on a celestial body to give the impression it has clouds moving independently to the ground

**Far ring**: Half of the ring circle that is farthest from the view

**Fullscreen tri**: Triangle that covers the entire screen for some passes

**Model**: Vertex and index streams that can be rendered with a single draw call

**Near ring**: Half of the ring circle that is nearest from the view

**Night texture**: Emissive texture seen on the dark side of a celestial body, e.g. city lights or lava

**Planet**: Celestial body (Sun, Planets, Moons), this is just shorter to write

**Specular**: Light reflected at a certain angle

# Texture streaming
## File structure
Textures can be split into multiple files for loading. When creating a stream texture, `filename` must point to a folder where a `info.sn` file must be present. This file contains something like this : 
```
size:2048
levels:2
prefix:""
separator:"_"
suffix:".DDS"
row_column_order:false
```
It describes how the tiles for this texture are stored in the file structure. Here are the rules:
* The `levels` field tells how many `/levelX/` folders there are (`N` being a number from `0` to `levels-1`)
* Each `levelN/` folder contains a number of DDS files named in the following fashion: `prefix + X + separator + Y + suffix` if `row_column_order` is `false`, `X` and `Y` are swapped otherwise. `X` and `Y` are the offset of the tiles from the top-left corner. Every DDS file must have the same format.
* The `level0/` folder contains one DDS file named in the above fashion with `X=0` and `Y=0`. The width of the DDS file must be exactly `size`, the height must be `size/2`, and all mipmaps down to 1x1 must be in the file.
* In `levelN/` folders where `N>0`, the files are `size*size` tiles, and `X` ranges from `0` to `2^N-1` and `Y` ranges from `0` to `2^(N-1)-1`. Each file should contain only one mipmap.
* Any missing file will result in an exception.

Example: With the above `info.sn` :
* The `level0/` folder contains a `2048x1024` DDS file with all mipmaps (12 layers)
* The `level1/` folder contains two `2048x2048` DDS files with one mipmap each, each named `0_0.DDS` and `1_0.DDS`.
* The overall size of the texture is then `4096x2048`, if we assemble all tiles of the topmost level.

## Streaming
The DDSStreamer class manages multi-threaded texture streaming:

An OpenGL buffer is allocated with pages of a certain size (as defined in the `init()` method), and mapped persistently. When loading a texture, all tile and mipmap info are put in a queue and ranges of the OpenGL buffer are assigned to this data. Flags and OpenGL fences ensure that the data is not stomped on in flight. In a separate thread, the DDS Loader gets the info in the queue and writes the data directly into the OpenGL buffer, in the ranges assigned (with the mapped pointer). The loading thread then signals the main thread by pushing data necessary for texture upload in another queue. The main thread then binds the OpenGL buffer as a PBO, calls `glTexImage*`, signals the fences and flips the flags of the ranges concerned. 

Stream textures work with handles so that transfers can be cancelled when a texture is deleted, avoiding 'zombie tranfers' on invalid texture names.

# Understanding the graphics pipeline
## Vertex data
### Planet vertex data
A vec4 for model-space position

A vec4 for texture coordinates (last 2 components untouched)
## Uniform Buffer Object structures
The order and type of UBO members should be chosen carefully so the std140 layouts match with c++ layouts (for direct copying).

Note : Due to the large distances between celestial bodies resulting in floating point imprecision, View and Model matrix are all translated by the negative of the camera position, effectively putting more precision closer to the camera. (View matrix are technically not translated; just built with view position of (0,0,0))
### Scene UBO
Contains:
* Projection matrix (mat4)
* View matrix (with eye position at (0,0,0,1)) (mat4)
* View position (always at (0,0,0,1)) (vec4)
* Ambient color (vec4)
* Inverse of exponent for gamma correction (float)
* "Exposure" (colors are multiplied by this value) (float)
* Far plane distance
* C coefficient for logarithmic depth calculation

### Planet UBO
Contains:
* Model matrix (but camera position is subtracted from planet position) (mat4)
* Atmosphere matrix (mat4)
* Far ring matrix (mat4)
* Near ring matrix (mat4)
* Planet position (view space) (vec4)
* Light direction (in view space) (vec4)
* Scattering constants (vec4)
* Color and hardness of specular reflection of mask 0 (vec4)
* Color and hardness of specular reflection of mask 1 (vec4)
* Ring plane normal vector (vec4)
* Inner ring distance
* Outer ring distance
* Intensity of star (float)
* X-position of cloud layer (float)
* Intensity of night texture (float)
* Radius of planet (float)
* Atmospheric height of planet (float)

### Flare UBO
Contains:
* Model matrix (mat4)
* Color (vec4)
* Brightness (float)

## Pipeline
First off, planets are put into two categories : close and far planets. Close planets are rendered as detailed spheres, while far planets are just rendered as flares.
### HDR pass
Opaque sections of close planets are rendered to a HDR multisampled rendertarget (without atmosphere and rings)
### Atmo pass
Translucent sections of close planets are rendered back-to-front to the same rendertarget
### Bloom pass
#### Highpass
The HDR multisampled rendertarget is resolved to a rendertarget where only pixels above a given threshold are kept (the others set to black).
#### Downscaling
The highpass rendertarget is then downscaled to 1/2, 1/4, 1/8 and 1/16 the size of the original rendertarget
#### Blurring
Each downscaled highpass rendertarget is blurred with a fixed kernel size and then added to the bigger one, and blurred again, and added again... until we stop at the 1/2 size rendertarget. The result is kept for later.
### Flares
Far planets are rendered as flares, with corona and halo effects to simulate the human eye.
### Tonemapping, resolve and presentation
Tonemap each sample, average them, add the bloom rendertarget on top and present.
