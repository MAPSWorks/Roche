## To Do

### Important stuff
- [ ] Text rendering with stb_truetype
- [ ] Automatic exposure
- [ ] Fix atmospheric shaders (fireflies, flickering at high distances) [find a way to get planet-origin coordinates without bouncing back from view space]

### Minor stuff
- [ ] Choose between frames of reference (absolute, axis towards sun, towards parent, following rotation), transition should be smooth and should only change the roll and reset pan angles.
- [ ] Better focus transitions (trace ray from view to target, if planet in the way move the camera along the shortest path to get no intersection, pan the camera towards the target at the same time)
- [ ] Frustum culling (only apply for rendering, not texture loading)
- [ ] Implementation of some "free cam" (look around, not constrained by parent body)
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
- [x] Flares for planets (data in planet UBO)
- [x] Better trackball controls (left click for move, wheel for distance, right click for pan, ?+wheel for fov)
- [x] Don't allow to skip transitions
- [x] Create atmo & ring textures at startup
- [x] Star map
- [x] Prettier flares
- [x] Wait for fences to unlock before tagging as complete
- [x] Add cost calculator to prevent too much texture updates at once
- [x] Add option to choose between synchronous texture loading (no pop-ins but stutters) or asynchronous (smoother framerate but pop-ins)
- [x] Better profiler
- [x] Change VAO / DrawCommand behavior for easier creation
- [x] Full warning check and comments
- [x] Toggleable bloom
- [x] Fix atmosphere artifacts at high distances
- [x] Better loading predictions
- [x] Put glViewport() where necessary

## Feature summary
