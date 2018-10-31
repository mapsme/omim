## Include dependence

                          qt
                           |
                 qt    generator_tool
                  |        |
                    map,  generator
       |          |           |       |        |
    search       eye      tracking   routing   drape_frontend
       |          |           |       /        |
       |          |           |    /           |
    editor      storage    traffic          drape
          \       |           |            /
            \     |     routing_common    /
              \   |           |          /
               \  |           |         /
                    ugc, transit
                        |
                     indexer
                        |
                     platform
                        |
                      coding
                        |
                     geometry
                        |
                       base
                        |
                      3party
                        |
                      std, boost

## Example of includes in cpp file

If a example.cpp file is located in some_dir dirctory it may have the following includes:
```c++
#include "some_dir/example.hpp"

#include "some_dir/base.hpp"
#include "some_dir/utils.hpp"

#include "generator/camera_info_collector.hpp"
#include "generator/feature_builder.hpp"
#include "generator/routing_helpers.hpp"

#include "map/gps_tracker.hpp"
#include "map/taxi_delegate.hpp"

#include "routing/route.hpp"
#include "routing/routing_helpers.hpp"

#include "search/locality_finder.hpp"
#include "search/reverse_geocoder.hpp"

#include "metrics/eye.hpp"

#include "storage/country_info_getter.hpp"
#include "storage/downloader_search_params.hpp"

#include "drape_frontend/route_renderer.hpp"
#include "drape_frontend/visual_params.hpp"

#include "drape/constants.hpp"

#include "editor/editable_data_source.hpp"

#include "traffic/traffic_info.hpp"

#include "routing_common/num_mwm_id.hpp"

#include "indexer/ftypes_sponsored.hpp"
#include "indexer/map_style_reader.hpp"
#include "indexer/scales.hpp"

#include "platform/platform.hpp"
#include "platform/platform_tests_support/scoped_dir.hpp"
#include "platform/platform_tests_support/scoped_file.hpp"
#include "platform/platform_tests_support/writable_dir_changer.hpp"

#include "coding/url_encode.hpp"
#include "coding/zip_reader.hpp"

#include "geometry/angles.hpp"
#include "geometry/any_rect2d.hpp"
#include "geometry/distance_on_sphere.hpp"

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
