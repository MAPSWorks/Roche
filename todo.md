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

## Feature summary
- [x] Now starts at the current date and time
- [x] Better trackball controls (left click for move, wheel for distance, right click for pan, alt+wheel for fov, ctrl+wheel for exposure)
- [x] More precise trajectories
- [x] Add option to choose between synchronous texture loading (no pop-ins but stutters) or asynchronous (smoother framerate but pop-ins)
- [x] Less atmosphere artifacts
- [x] Changed how flares looked, settled on a more "camera" feel
- [x] Background star map
- [x] Textual info displayed on screen
- [x] Better performance
- [x] Misc improvements
