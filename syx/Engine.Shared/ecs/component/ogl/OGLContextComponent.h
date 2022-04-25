#pragma once

constexpr size_t OGL_THREAD = 0;

//View this with write access in systems that do OGL commands to ensure scheduler orders the systems linearly
struct OGLContextComponent {};