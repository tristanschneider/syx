# dof

`dof` is a small scoped 2D game using "data oriented framework". It is intended to be the simplest possible architecture to get up and running and is about seeing the pros and cons of keeping it simple as opposed to jumping straight to an obvious solution like ECS. It is a top down 2D game where the player uses physics based abilities to push large amounts of picture fragments back into place like a jigsaw puzzle. Ultimately this assembles a series of pictures that tell a story.

### Prerequisites

- [git](https://git-scm.com/download/win) Or any git gui
- [Visual Studio](https://visualstudio.microsoft.com/downloads/) (I'm on 2022, anything that or later should work)
- [CMake](https://cmake.org/download/) (I'm on 3.20.3, minimum 3.10, 3.6 and the startup project will be set properly)
- [ispc](https://ispc.github.io/downloads.html) (I'm on 1.15.0) Make sure the bin folder is in your %path%

### Building dof

- Open a command prompt
- Clone the repository `git clone https://github.com/tristanschneider/syx.git`
- Go to dof `cd syx\dof`
- Initialize submodules `git submodule update --init --recursive`
- Generate the project `cmake .`
- Open the solution in visual studio `start dof.sln`
- F5 build and run the project using startup project `win32`.
  - If on a version of cmake before 3.6, select `win32` in the solution explorer, right click, "set as startup project"
