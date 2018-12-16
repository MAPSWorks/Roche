# Roche
Solar system simulation in OpenGL 4.5

## Synopsis
A pretty Solar System simulation with two-body physics. Uses modern features of OpenGL for a realtime experience.

## Showcase
![](http://i.imgur.com/4ddKzZl.png)

![](http://i.imgur.com/7sQgzMr.png)

![](http://i.imgur.com/5dAdh68.png)

[Full Image album](http://imgur.com/a/SJVQz)

## Installation
* Check that OpenGL 4.5 is supported on your computer ([nice tool](https://www.saschawillems.de/?page_id=771) if you're unsure)
* Download the [latest release](https://github.com/leluron/Roche/releases/latest)
* Run it

## Usage
* Hold left click and move to look around
* Mouse wheel to zoom in and out
* Hold right click and move to pan the view around
* Alt + Mouse wheel to change field of view
* Ctrl + Mouse wheel to change exposure
* Tab to cycle through celestial bodies
* Shift+Tab to cycle back
* K/L to change timewarp speed
* Escape to exit
### Advanced
* F5 to print profiling info to command line
* F12 to save a screenshot to `screenshot/` folder
* B to toggle bloom
* W to toggle wireframe mode

## Build
Requirements:
* [CMake](https://cmake.org)
* A C++11 compiler
* [GLFW](https://github.com/glfw/glfw)
* [GLEW](https://github.com/nigels-com/glew)
* [GLM](https://github.com/g-truc/glm)

Out of source build:
```
mkdir -p build
cd build
cmake ..
make
```

You still need custom textures or the textures distributed with the latest release, as they are too big to be contained in the repo. Beware of the incompatibilities.

## Contributors
* [@leluron](https://github.com/leluron)
* [@ablanleuil](https://github.com/ablanleuil) for [SHAUN](https://github.com/ablanleuil/SHAUN), ideas and inspiration
