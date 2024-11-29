# Table Framework

- Create thread-local instances of the game database that can be used to queue table modification
  - Create an write to all desired rows
  - Delete can write to a log for requested deletes equivalent to how DBEvents does now
  - Migrate is tricky. Could use log approach for plain moves. Move+modify would require copying all rows into the local database which would be expensive. Copying only modified rows is an option, which then needs to be part of the log to identify.
  - This requires modifying the interface for tasks to formally specify a factory object rather than capturing arbitrary dependencies in a callback
- Migrate stat effects to use the local tables
- Easier way to manage a copy of a table like renderables. Could be done with a table-scoped version so copies are skipped if on the same version.

# Graphics

- Switch to abstraction, probably sokol
- Mesh shapes

# Platform

- Choose a framework, probably also sokol

# Gameplay

- Prevent fragments from wandering off the edge of the map
  - Could either look forward for lack of ground or put collisions on a navigation-only layer near the border. Currently favoring the latter for simplicity

# Physics

- Mesh shapes, spheres