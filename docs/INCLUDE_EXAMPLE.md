# Example of includes in cpp file

If a example.cpp file is located in some_dir dirctory it may have the following includes:
```c++
#include "some_dir/example.hpp"

#include "some_dir/base.hpp"
#include "some_dir/utils.hpp"

#include "generator/camera_info_collector.hpp"
#include "generator/feature_builder.hpp"
#include "generator/routing_helpers.hpp"

#include "platform/platform.hpp"
#include "platform/platform_tests_support/scoped_dir.hpp"
#include "platform/platform_tests_support/scoped_file.hpp"
#include "platform/platform_tests_support/writable_dir_changer.hpp"

#include "base/assert.hpp"
#include "base/geo_object_id.hpp"

#include "defines.hpp"
#include "private.hpp"

#include <algorithm>
#include <fstream>
#include <utitlity>

#include "boost/algorithm/string/replace.hpp"
#include "boost/range/adaptor/map.hpp"
#include "boost/range/algorithm/copy.hpp"

#include "3party/jansson/jansson_handle.hpp"
```
