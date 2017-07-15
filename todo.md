## To Do

### Important stuff

### Minor stuff
- [ ] Investigate zoom in slowdown
- [ ] Investigate z fighting
- [ ] Better loading predictions
- [ ] Completeness for mipmap levels
- [ ] Frustum culling
- [ ] Compute log average luminance for auto exposure
- [ ] Correct luminance values

### Stuff for later
- [ ] Support custom models (asteroids, phobos, deimos)
- [ ] Some kind of HUD (imgui)
- [ ] Vulkan implementation, one day

## Done

## Feature summary
- [x] Texture streaming w/ PBOs, split files and latest BC formats
- [x] Better performance bloom, w/ reduction of fireflies, true bloom curve
- [x] More parametrizable log depth, more precision
- [x] Tessellation when looking at close bodies
- [x] Bodies can have specular reflection and masks, e.g. ocean reflections
- [x] Higher resolution and better compression on most textures
- [x] Rings cast shadows on the parent body
- [x] Misc design improvements
