# Table Framework

- Easier way to manage a copy of a table like renderables. Could be done with a table-scoped version so copies are skipped if on the same version.
- Allow migrate+modify operations through thread-local DB? Tricky and inefficient so probably better albeit cumbersome to manually do via events.

# Asset Pipeline

- Load into a RuntimeDatabase created based on the keys in the scene then match those against the game database for instantiation.

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