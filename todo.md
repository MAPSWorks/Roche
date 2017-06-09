## To Do
- [ ] Invalidate framebuffers
- [ ] Better log depth buffer
- [ ] Correct flares
- [ ] Use BC7 for textures
- [ ] Specular support
- [ ] Vulkan implementation, one day

## Done
- [x] Lower res spheres + tesselation (also for rings)
- [x] Better texture streaming management
- [x] Isolate screenshot and texture streaming functionality
- [x] Change depth clip control to be from zero to one
- [x] Clean up planet structures
- [x] Better ddsloader API
- [x] Remove grouped mipmap loading from ddsloader
- [x] Load highest mipmap instead of default color for stream textures
- [x] Better screenshot thread
- [x] Move screenshot functionality into renderer
- [x] Use lambdas for complex initialization
- [x] Separate shaders & shader pipelines
- [x] Proper move semantics & const ref params
- [x] Change skipMipmap with maxSize
- [x] Proper CMake compile features
- [x] Move profiler to own file
- [x] Tell GL to do gamma correction itself
- [x] Override wherever possible
- [x] Remove SSAA option
- [x] Change gl_util to avoid memory copying (write/read directly)
