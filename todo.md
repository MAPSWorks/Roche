## To Do

### Important stuff
- [ ] Text rendering with stb_truetype
- [ ] Automatic exposure
- [ ] Fix atmospheric shaders (fireflies, flickering at high distances) [find a way to get planet-origin coordinates without bouncing back from view space]

### Minor stuff
- [ ] Frustum culling (only apply for rendering, not texture loading)
- [ ] Fix atmosphere when view inside (gpu gems trick by subtracting rays)

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

## Feature summary
- [x] Add option to choose between synchronous texture loading (no pop-ins but stutters) or asynchronous (smoother framerate but pop-ins)
- [x] Better trackball controls (left click for move, wheel for distance, right click for pan, alt+wheel for fov, ctrl+wheel for exposure)
- [x] Less atmosphere artifacts
- [x] Changed how flares looked, settled on a more "camera" feel
- [x] Background star map
- [x] Better performance
- [x] Misc improvements
