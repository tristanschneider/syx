- Task should take an async handle (std future?), the task itself shouldn't be the handle as it is now
- Find a way to make game object modification less confusing when not done through lua. The message dependencies and order aren't obvious.
- Split LuaNode between reflection and actual lua functionality. It provides an interface to interact with types so it's possible to generically copy and mutate them. On top of this are lua operations, but they're mixed in with the interface. It should be split apart so that extensions like lua can use it without it having to go directly on the interface. For instance, what if a different scripting implementation wanted to use it?
- Formalize message response sending and callbacks.
- Allow more general message queues beyond the ones tied to the app frame. It should be possible for systems to expose their own channels that they can choose to process whenever they want.