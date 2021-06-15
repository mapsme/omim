To build the Mapsme plugin for osrm-backend:
============================================

0. Build the regular omim project in the directory ${OMIM_BUILD_PATH}.

1. Ignore all cmake files except those in omim/3party/osrm/osrm-backend, i.e.
   run

   > cd osrm-backend-build
   > cmake ${OMIM_ROOT}/3party/osrm/osrm-backend/ -DOMIM_BUILD_PATH=${OMIM_BUILD_PATH}

2. Build the usual OSRM tools.

   > make -k -j4 osrm-extract osrm-prepare osrm-routed

3. Run the OSRM pipeline on an *.osm.pbf file, see project OSRM docs or use
   omim/tools/unix/generate_planet_routing.sh to do it automatically.

4. Run the routing daemon (osrm-routed).

5. The mapsme API that current version of the repository expects is

   GET /{service}?loc={latlon1}&loc={latlon2}

   where {latlon1} and {latlon2} are {coordinates} of source and destination points, respectively, in the usual OSRM format.

   For example, if osrm-routed runs on localhost on the default port 5000, use

   > curl "http://127.0.0.1:5000/mapsme?loc=55,37&loc=55,40"

   The expected response is (note the omim-mercator coordinates)
   {"used_mwms":[[36.140419,60.524949,"Russia_Kursk Oblast"],[37.872379,67.58203,"Russia_Moscow Oblast_East"],[37.557854,67.355247,"Russia_Moscow"]]}




LUABIND
=======

If you youse luabind library with a version lesser than 0.9.2,
you may have troubles compiling OSRM code with a boost >=1.57
(more details here: https://github.com/luabind/luabind/issues/27 ).

You can fix it either by using latest luabind from source https://github.com/rpavlik/luabind
or https://github.com/DennisOSRM/luabind, or by applying the following patch
to /usr/include/luabind/object.hpp:
https://github.com/alex85k/luabind/commit/2340928e00f4d606255dd5d570e1e142353a5fdd


On macOS, luabind is no longer supported by Homebrew so you may need to

  > brew edit luabind

and comment out the "disable!" line.
