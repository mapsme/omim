#pragma once

#ifdef new
#undef new
#endif

#include <initializer_list>
using std::initializer_list;

#ifdef DEBUG_NEW
#define new DEBUG_NEW
#endif
