## To Do

### Important stuff
- [ ] Fix atmospheric shaders (fireflies, flickering at high distances) [find a way to get planet-origin coordinates without bouncing back from view space]

### Minor stuff
- [ ] Fix planet orientations at epoch
- [ ] Display some kind of planet description
- [ ] Split up other textures
- [ ] Opening screen with controls (can be closed with any input after loading is done)
- [ ] Do a compute depth buffer test instead of occlusion query for sun occlusion

### Stuff for later
- [ ] Earth water moving normal map
- [ ] Support custom models (asteroids, phobos, deimos)
- [ ] Heightmap applied in tese shader
- [ ] Vulkan implementation, one day
- [ ] Auto-pilot tour

## Done
- [x] Make a assign+write method in Buffer
- [x] Remove fullscreen tri model
- [x] Enforce the _member notation
- [x] Separate barycenters from bodies
- [x] Huge refactor for entities
- [x] Rename 'planet' with 'entity' or 'body'
- [x] Add barycenters
- [x] Ecliptic reference frame instead of equator
- [x] Get more recent orbital parameters
- [x] Body name displayed on top left, with mention of parent body and time
- [x] Text rendering with stb_truetype
- [x] Changed for a whiter sun texture
- [x] Fix atmosphere when view inside
- [x] Frustum culling (only apply for rendering, not texture loading)

## Feature summary
- [x] Add option to choose between synchronous texture loading (no pop-ins but stutters) or asynchronous (smoother framerate but pop-ins)
- [x] Better trackball controls (left click for move, wheel for distance, right click for pan, alt+wheel for fov, ctrl+wheel for exposure)
- [x] Less atmosphere artifacts
- [x] Changed how flares looked, settled on a more "camera" feel
- [x] Background star map
- [x] Better performance
- [x] Misc improvements
