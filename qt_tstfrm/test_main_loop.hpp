#pragma once

#include <functional>

class QPaintDevice;
typedef std::function<void (QPaintDevice *)> TRednerFn;
void RunTestLoop(char const * testName, TRednerFn const & fn, bool autoExit = true);
