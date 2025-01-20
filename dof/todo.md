# Table Framework

- Modify the interface for tasks to formally specify a factory object rather than capturing arbitrary dependencies in a callback
- Replace DBEvents with sparse row(s?) that indicate the desired change. This makes event processing more efficient as it's scoped to the tables that change. Also might be a bit more intuitive/consistent with the rest of the framework.
  - Deletion can add a flag to the element
  - Move can add a sparse element with a table id
  - New can add a flag to the element
  - Might be a bit more confusing in terms of what table is expected in "pre" vs "post" events
- - Easier way to manage a copy of a table like renderables. Could be done with a table-scoped version so copies are skipped if on the same version.
- Allow migrate+modify operations through thread-local DB? Tricky and inefficient so probably better albeit cumbersome to manually do via events.

# Graphics

- Combine tables that use the same mesh/texture into the same draw call
- Texture atlas

# Platform

# Gameplay

- Prevent fragments from wandering off the edge of the map
  - Could either look forward for lack of ground or put collisions on a navigation-only layer near the border. Currently favoring the latter for simplicity
- Player change density
- Melee swing ability
- Charge ability
- Grapple ability
- Grab ability
  - Touch fragment to start, any others touching this get stuck too, spin to throw, maybe pull in fragments gently
- Control fragment by jumping on it

# Physics

- Mesh shapes, spheres
- Add dt
- Add LOD via broadphase bounds that the rest of the game uses to rate limit itself