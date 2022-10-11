export void integrateLinearPosition(uniform float position[],
	uniform float velocity[],
	uniform float dt,
	uniform int count) {
	foreach(i = 0 ... count) {
		position[i] += velocity[i]*dt;
	}
}