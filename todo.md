## To Do

### Important stuff
- [ ] Text rendering with stb_truetype
- [ ] Fix atmospheric shaders (fireflies, flickering at high distances) [find a way to get planet-origin coordinates without bouncing back from view space]

### Minor stuff
- [ ] Split up other textures
- [ ] Do a compute depth buffer test instead of occlusion query for sun occlusion

#### Once text rendering available
- [ ] Opening screen with controls (can be closed with any input after 1 sec), waits for textures to load
- [ ] Body name displayed on top left, with mention of parent body and time
- [ ] Names of distant planets next to them (fade in when planet into view for 1~2sec)

### Stuff for later
- [ ] Support custom models (asteroids, phobos, deimos)
- [ ] Heightmap applied in tese shader
- [ ] Vulkan implementation, one day
- [ ] Auto-pilot tour

## Done
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
