# Bugs

- Mesh collision sometimes applies incorrect impulse

# Table Framework

- Easier way to manage a copy of a table like renderables. Could be done with a table-scoped version so copies are skipped if on the same version.
- Allow migrate+modify operations through thread-local DB? Tricky and inefficient so probably better albeit cumbersome to manually do via events.

# Asset Pipeline

# Graphics

- Combine tables that use the same mesh/texture into the same draw call
- Texture atlas (or could be done on the content side by configuring the textures that the scene uses)

# Platform

- Diagnostics
  - logging
  - debug entry like imgui selection, usable everywhere

# Gameplay

- Add Z to fragments
  - Make them respawn when they fall off the edge
- Player change density
- Melee swing ability
- Charge ability
- Grapple ability
- Grab ability
  - Touch fragment to start, any others touching this get stuck too, spin to throw, maybe pull in fragments gently
- Control fragment by jumping on it

# Physics

- Add dt
- Add LOD via broadphase bounds that the rest of the game uses to rate limit itself
- Predictive collision radius