## To Do

### Important stuff
- [ ] Specular support
- [ ] Ring shadow on planet
- [ ] Don't use stream textures when incomplete
- [ ] Convert all textures to BC7 or BC4

### Minor stuff
- [ ] Frustum culling
- [ ] Compute log average luminance for auto exposure
- [ ] Correct luminance values

### Stuff for later
- [ ] Support custom models (asteroids, phobos, deimos)
- [ ] Investigate slowdown when zooming in
- [ ] Some kind of HUD
- [ ] Vulkan implementation, one day

## Done
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
