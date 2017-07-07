## To Do

### Important stuff

### Minor stuff
- [ ] Frustum culling
- [ ] Compute log average luminance for auto exposure
- [ ] Correct luminance values

### Stuff for later
- [ ] Support custom models (asteroids, phobos, deimos)
- [ ] Some kind of HUD (imgui)
- [ ] Vulkan implementation, one day

## Done
- [x] Convert all textures to BC7 or BC4
- [x] Ring shadow on planet
- [x] Specular support
- [x] Don't use stream textures when incomplete
- [x] Wireframe option
- [x] Shader API with caching
- [x] BC4 and BC5 loading through old fourCC codes
- [x] Better DDS Streamer partitioning
- [x] Set max tex size in DDS Streamer

## Feature summary
- [x] Texture streaming w/ PBOs, split files and latest BC formats
- [x] Better performance bloom, w/ reduction of fireflies, true bloom curve
- [x] More parametrizable log depth, more precision
- [x] Tessellation
- [x] Misc design improvements
