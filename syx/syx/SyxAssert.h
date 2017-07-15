#pragma once
#include <cassert>
//This is pretty useless right now since I don't know how I want to assert,
//but at least all assertions will go through here so I can do something more sophisticated later
#define SyxAssertWarning(condition, ...) assert(condition)
#define SyxAssertError(condition, ...) assert(condition)

