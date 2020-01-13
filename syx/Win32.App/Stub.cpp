#include "Precompile.h"

// It appears that exe projects in visual studio are special. This project must be empty because if it includes
// any shared projects it breaks intellisense for files. Because of this, Win32.App needs to be empty while Win32.Platform
// imports the actual Win32.Shared items, and references the rest of the engine.
// On top of this, a stub file is needed for this project to be able to find and link to main in Win32.App.