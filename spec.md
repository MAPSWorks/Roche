# Terminology
**Cloud texture**: Translucent texture mapped on a celestial body to give the impression it has clouds moving independently to the ground

**Far ring**: Half of the ring circle that is farthest from the view

**Fullscreen tri**: Triangle that covers the entire screen for some passes

**Model**: Vertex and index streams that can be rendered with a single draw call

**Near ring**: Half of the ring circle that is nearest from the view

**Night texture**: Emissive texture seen on the dark side of a celestial body, e.g. city lights or lava

**Planet**: Celestial body (Sun, Planets, Moons), this is just shorter to write

**Specular**: Light reflected at a certain angle

**Stream texture**: Texture whose image is to be loaded in a different thread

# Texture streaming
To avoid creating different OpenGL contexts, only the texture loading from disk is done in a separate thread. Texture uploading is done in the main thread after the loading has completed.

Therefore, there is two separate queues : the **Wait Queue** and the **Loaded Queue**: the main thread submits one or multiple mipmap levels to the Wait Queue. The loading thread then empties the Wait Queue and loads all mipmap levels from disk. It then submit that info (dimensions and pixel data) to the Loaded Queue. The main thread periodically checks the Loaded Queue for loaded mipmap levels and updates the textures accordingly.

Note for later: Use PBOs.

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

### Planet UBO
Contains:
* Model matrix (but camera position is subtracted from planet position) (mat4)
* Atmosphere matrix (mat4)
* Far ring matrix (mat4)
* Near ring matrix (mat4)
* Planet position (view space) (vec4)
* Light direction (in view space) (vec4)
* Scattering constants (vec4)
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
Tonemap each sample, average them and present.



