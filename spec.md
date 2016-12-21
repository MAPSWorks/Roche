# Terminology
**Attachment**: Output of a render pass

**Cloud texture**: Alpha texture mapped on a celestial body to give the impression it has clouds moving independently to the ground

**Diffuse**: Light reflected in all directions, as opposed to specular reflection. In the case of no lighting (skybox and sun), the apparent color of the material.

**Fullscreen tri**: Triangle that covers the entire screen for deferred passes

**Model**: Vertex and index streams that can be rendered with a single draw call

**Night texture**: Emissive texture seen on the dark side of a celestial body, e.g. city lights or lava

**Planet**: Celestial body (Sun, Planets, Moons), this is just shorter to write

**Skybox**: Background object

**Specular** Light reflected at a certain angle

**UBO**: Uniform Buffer Object

**UV**: Texture coordinates

**UV derivatives**: Rate of change of UVs on a given pixel, to know what level of detail we need for a texture

# Understanding the graphics pipeline
## Vertex data
### Planet vertex data
A vec4 for model-space position

A vec4 for texture coordinates (last 2 components untouched)
## Uniform Buffer Object structures
Note : Due to the large distances between celestial bodies resulting in floating point imprecision, View and Model matrix are all translated by the negative of the camera position, effectively putting more precision closer to the camera. (View matrix are technically not translated; just built with view position of (0,0,0))
### Dynamic Scene UBO
Contains:
* Projection matrix (mat4)
* View matrix (with eye position at (0,0,0,1)) (mat4)
* View position (always at (0,0,0,1)) (vec4)
* Inverse of exponent for gamma correction (float)

### Dynamic Planet UBO
Contains:
* Model matrix (but camera position is subtracted from planet position) (mat4)
* Light direction (normalized vector from planet to sun) (vec4)

## Buffers
There are only two buffers: one for static data (upload once at init) and the other for dynamic data (upload every frame, triple buffered)
### Static buffer
Contains vertex and index data for the fullscreen tri (for deferred rendering), a sphere (default planet model), and custom planet models. All vertex data is put in the first part of the buffer, and index data in the second part. Models contain the offsets to their respective vertex and index data.

### Dynamic buffer
Contains one Dynamic Scene UBO, and one Dynamic Planet UBO for each planet. This is triple buffered, so the update should occur for the next frame.

## Textures
The skybox, sun and planets all use a diffuse texture. Planets can use a cloud texture but those who don't will use a default 1x1 texture meaning no clouds. Same thing for night textures. Textures are stored on disk with all their mipmaps and compressed with BC3 (DXT5) in DDS files. When the view comes close to any planet (implementation defined) the texture will be loaded and used for this planet.

## Rendering passes
Rendering happens in multiple passes :
* G-Buffer pass
* HDR pass
* Resolve pass

### G-Buffer pass
Attachments:
* Linear depth: Used to reconstruct fragment position in later passes (1channel, 32bpp floating point, temporary)
* UVs (2channels, 32bpp fixed point)
* UV derivatives (4channels, 64bpp floating point)
* Normals (2channels, 32bpp floating point, compressed with [spheremap transform](http://aras-p.info/texts/CompactNormalStorage.html#method04spheremap))
* Depth/Stencil buffer (24_8 bpp)

All the scene is rendered from front to back, to minimize overdraw. Each planet will write a unique stencil value for the next pass. Depth is logarithmic to deal with the huge range of depth values.

### HDR pass
Attachments:
* HDR rendertarget (3 channels, 48bpp , floating point)
* Same Depth/Stencil buffer as before

For each planet, the corresponding shaders and UBO are bound and stencil testing is enabled on that previous stencil value. Full lighting operations are written in the hdr rendertarget.

Forward drawing operations can still be performed for later transparency effects (rings, atmospheres)

### Resolve pass
Named 'resolve' because it was meant to resolve the previously multisampled HDR rendertarget, but the idea was scraped.

Attachments:
* Default framebuffer/ Swapchain

Tonemap/gamma correct the HDR buffer

