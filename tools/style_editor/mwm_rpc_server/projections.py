# -*- coding: utf-8 -*-
#    This file is part of twms.
# This program is free software. It comes without any warranty, to
# the extent permitted by applicable law. You can redistribute it
# and/or modify it under the terms of the Do What The Fuck You Want
# To Public License, Version 2, as published by Sam Hocevar. See
# http://www.wtfpl.net/ for more details.

import math

try:
    import pyproj
except ImportError:
    class pyproj:

        class Proj:

            def __init__(self, pstring):
                self.pstring = pstring

        def transform(self, pr1, pr2, c1, c2):
            if pr1.pstring == pr2.pstring:
                return c1, c2
            else:
                raise NotImplementedError(
                    "Pyproj is not installed - can't convert between projectios. Install pyproj please.")


projs = {
    "EPSG:4326": {
        "proj":
        pyproj.Proj(
            "+proj=longlat +ellps=WGS84 +datum=WGS84 +no_defs"),
        "bounds": (-180.0, -90.0, 180.0, 90.0),
    },
    "NASA:4326": {
        "proj":
        pyproj.Proj(
            "+proj=longlat +ellps=WGS84 +datum=WGS84 +no_defs"),
        "bounds": (-180.0, -166.0, 332.0, 346.0),
    },
    "EPSG:3395": {
        "proj":
        pyproj.Proj(
            "+proj=merc +lon_0=0 +k=1 +x_0=0 +y_0=0 +ellps=WGS84 +datum=WGS84 +units=m +no_defs"),
        "bounds": (-180.0, -85.0840591556, 180.0, 85.0840590501),
    },
    "EPSG:3857": {
        "proj":
        pyproj.Proj(
            "+proj=merc +lon_0=0 +lat_ts=0 +x_0=0 +y_0=0 +a=6378137 +b=6378137 +units=m +no_defs"),
        "bounds": (-180.0, -85.0511287798, 180.0, 85.0511287798),
    },
    "EPSG:32635": {
        "proj":
        pyproj.Proj(
    "+proj=utm +zone=35 +ellps=WGS84 +datum=WGS84 +units=m +no_defs"),
        "bounds": (-180.0, -90.0, 180.0, 90.0),
    },
    "EPSG:32636": {
        "proj":
        pyproj.Proj(
            "+proj=utm +zone=36 +ellps=WGS84 +datum=WGS84 +units=m +no_defs"),
        "bounds": (-180.0, -90.0, 180.0, 90.0),
    },
    "EPSG:32637": {
        "proj":
        pyproj.Proj(
            "+proj=utm +zone=37 +ellps=WGS84 +datum=WGS84 +units=m +no_defs"),
        "bounds": (-180.0, -90.0, 180.0, 90.0),
    },
    "EPSG:32638": {
        "proj":
        pyproj.Proj(
    "+proj=utm +zone=38 +ellps=WGS84 +datum=WGS84 +units=m +no_defs"),
        "bounds": (-180.0, -90.0, 180.0, 90.0),
    },
    "EPSG:32639": {
        "proj":
        pyproj.Proj(
    "+proj=utm +zone=39 +ellps=WGS84 +datum=WGS84 +units=m +no_defs"),
        "bounds": (-180.0, -90.0, 180.0, 90.0),
    },
    "EPSG:32640": {
        "proj":
        pyproj.Proj(
    "+proj=utm +zone=40 +ellps=WGS84 +datum=WGS84 +units=m +no_defs"),
        "bounds": (-180.0, -90.0, 180.0, 90.0),
    },
    "EPSG:32641": {
        "proj":
        pyproj.Proj(
    "+proj=utm +zone=41 +ellps=WGS84 +datum=WGS84 +units=m +no_defs"),
        "bounds": (-180.0, -90.0, 180.0, 90.0),
    },
    "EPSG:32642": {
        "proj":
        pyproj.Proj(
            "+proj=utm +zone=42 +ellps=WGS84 +datum=WGS84 +units=m +no_defs"),
        "bounds": (-180.0, -90.0, 180.0, 90.0),
    },
}
proj_alias = {
    "EPSG:900913": "EPSG:3857",
    "EPSG:3785": "EPSG:3857",
}


def _c4326t3857(t1, t2, lon, lat):
    """
    Pure python 4326 -> 3857 transform. About 8x faster than pyproj.
    """
    lat_rad = math.radians(lat)
    xtile = lon * 111319.49079327358
    ytile = math.log(math.tan(lat_rad) + (1 / math.cos(lat_rad))) / \
        math.pi * 20037508.342789244
    return(xtile, ytile)


def _c3857t4326(t1, t2, lon, lat):
    """
    Pure python 3857 -> 4326 transform. About 12x faster than pyproj.
    """
    xtile = lon / 111319.49079327358
    ytile = math.degrees(
        math.asin(math.tanh(lat / 20037508.342789244 * math.pi)))
    return(xtile, ytile)


def _c4326t3395(t1, t2, lon, lat):
    """
    Pure python 4326 -> 3395 transform. About 8x faster than pyproj.
    """
    E = 0.0818191908426
    A = 20037508.342789
    F = 53.5865938
    tmp = math.tan(0.78539816339744830962 + math.radians(lat) / 2.0)
    pow_tmp = math.pow(
        math.tan(0.78539816339744830962 +
                 math.asin(E * math.sin(math.radians(lat))) / 2.0),
        E)
    x = lon * 111319.49079327358
    y = 6378137.0 * math.log(tmp / pow_tmp)
    return (x, y)


def _c3395t4326(t1, t2, lon, lat):
    """
    Pure python 4326 -> 3395 transform. About 3x faster than pyproj.
    """
    r_major = 6378137.000
    temp = 6356752.3142 / 6378137.000
    es = 1.0 - (temp * temp)
    eccent = math.sqrt(es)
    ts = math.exp(-lat / r_major)
    HALFPI = 1.5707963267948966
    eccnth = 0.5 * eccent
    Phi = HALFPI - 2.0 * math.atan(ts)
    N_ITER = 15
    TOL = 1e-7
    i = N_ITER
    dphi = 0.1
    while ((abs(dphi) > TOL) and (i > 0)):
        i -= 1
        con = eccent * math.sin(Phi)
        dphi = HALFPI - 2.0 * \
            math.atan(ts * math.pow((1.0 - con) / (1.0 + con), eccnth)) - Phi
        Phi += dphi

    x = lon / 111319.49079327358
    return (x, math.degrees(Phi))


pure_python_transformers = {
    ("EPSG:4326", "EPSG:3857"): _c4326t3857,
    ("EPSG:3857", "EPSG:4326"): _c3857t4326,
    ("EPSG:4326", "EPSG:3395"): _c4326t3395,
    ("EPSG:3395", "EPSG:4326"): _c3395t4326,
}


def tile_by_bbox(bbox, zoom, srs="EPSG:3857"):
    """
    Converts bbox from 4326 format to tile numbers of given zoom level, with correct wraping around 180th meridian
    """
    a1, a2 = tile_by_coords((bbox[0], bbox[1]), zoom, srs)
    b1, b2 = tile_by_coords((bbox[2], bbox[3]), zoom, srs)
    if b1 < a1:
        b1 += 2 ** (zoom - 1)
    return a1, a2, b1, b2


def bbox_by_tile(z, x, y, srs="EPSG:3857"):
    """
    Tile numbers of given zoom level to EPSG:4326 bbox of srs-projected tile
    """
    a1, a2 = coords_by_tile(z, x, y, srs)
    b1, b2 = coords_by_tile(z, x + 1, y + 1, srs)
    return a1, b2, b1, a2


def coords_by_tile(z, x, y, srs="EPSG:3857"):
    """
    Converts (z,x,y) to coordinates of corner of srs-projected tile
    """
    z -= 1
    normalized_tile = (x / (2. ** z), 1. - (y / (2. ** z)))
    projected_bounds = from4326(projs[proj_alias.get(srs, srs)]["bounds"], srs)
    maxp = [projected_bounds[2] - projected_bounds[0],
            projected_bounds[3] - projected_bounds[1]]
    projected_coords = [(
                        normalized_tile[0] * maxp[0]) + projected_bounds[0],
                        (normalized_tile[1] * maxp[1]) + projected_bounds[1]]
    return to4326(projected_coords, srs)


def tile_by_coords(xxx_todo_changeme, zoom, srs="EPSG:3857"):
    """
    Converts EPSG:4326 latitude and longitude to tile number of srs-projected tile pyramid.
    lat, lon - EPSG:4326 coordinates of a point
    zoom - zoomlevel of tile number
    srs - text string, specifying projection of tile pyramid
    """
    (lon, lat) = xxx_todo_changeme
    zoom -= 1
    projected_bounds = from4326(projs[proj_alias.get(srs, srs)]["bounds"], srs)
    point = from4326((lon, lat), srs)
    point = [point[0] - projected_bounds[0], point[1] - projected_bounds[1]]
        # shifting (0,0)
    maxp = [projected_bounds[2] - projected_bounds[0],
            projected_bounds[3] - projected_bounds[1]]
    point = [1. * point[0] / maxp[0], 1. * point[1] / maxp[1]]
        # normalizing
    return point[0] * (2 ** zoom), (1 - point[1]) * (2 ** zoom)


def to4326(line, srs="EPSG:3857"):
    """
    Wrapper around transform call for convenience. Transforms line from srs to EPSG:4326
    line - a list of [lat0,lon0,lat1,lon1,...] or [(lat0,lon0),(lat1,lon1),...]
    srs - text string, specifying projection
    """
    return transform(line, srs, "EPSG:4326")


def from4326(line, srs="EPSG:3857"):
    """
    Wrapper around transform call for convenience. Transforms line from EPSG:4326 to srs
    line - a list of [lat0,lon0,lat1,lon1,...] or [(lat0,lon0),(lat1,lon1),...]
    srs - text string, specifying projection
    """
    return transform(line, "EPSG:4326", srs)


def transform(line, srs1, srs2):
    """
    Converts a bunch of coordinates from srs1 to srs2.
    line - a list of [lat0,lon0,lat1,lon1,...] or [(lat0,lon0),(lat1,lon1),...]
    srs[1,2] - text string, specifying projection (srs1 - from, srs2 - to)
    """

    srs1 = proj_alias.get(srs1, srs1)
    srs2 = proj_alias.get(srs2, srs2)
    if srs1 == srs2:
        return line
    if (srs1, srs2) in pure_python_transformers:
        func = pure_python_transformers[(srs1, srs2)]
        # print "pure"
    else:

        func = pyproj.transform
    line = list(line)
    serial = False
    if (not isinstance(line[0], tuple)) and (not isinstance(line[0], list)):
        serial = True
        l1 = []
        while line:
            a = line.pop(0)
            b = line.pop(0)
            l1.append([a, b])
        line = l1
    ans = []
    pr1 = projs[srs1]["proj"]
    pr2 = projs[srs2]["proj"]
    for point in line:
        p = func(pr1, pr2, point[0], point[1])
        if serial:
            ans.append(p[0])
            ans.append(p[1])
        else:
            ans.append(p)
    return ans


if __name__ == "__main__":
    import debug
    print _c4326t3857(1, 2, 27.6, 53.2)
    print from4326((27.6, 53.2), "EPSG:3857")
    a = _c4326t3857(1, 2, 27.6, 53.2)
    print to4326(a, "EPSG:3857")
    print _c3857t4326(1, 2, a[0], a[1])
    print "3395:"
    print _c4326t3395(1, 2, 27.6, 53.2)
    print from4326((27.6, 53.2), "EPSG:3395")
    a = _c4326t3395(1, 2, 27.6, 53.2)
    print to4326(a, "EPSG:3395")
    print _c3395t4326(1, 2, a[0], a[1])

    a = debug.Timer("Pure python 4326<3857")
    for i in xrange(0, 100000):
        t = _c3857t4326(1, 2, 3072417.9458943508, 7020078.5326420991)
    a.stop()
    a = debug.Timer("TWMS wrapped 4326<3857")
    for i in xrange(0, 100000):
        t = to4326((3072417.9458943508, 7020078.5326420991), "EPSG:3857")
    a.stop()
    a = debug.Timer("Pyproj unwrapped 4326<3857")
    pr1 = projs["EPSG:3857"]["proj"]
    pr2 = projs["EPSG:4326"]["proj"]
    for i in xrange(0, 100000):
        t = pyproj.transform(pr1, pr2, 3072417.9458943508, 7020078.5326420991)
    a.stop()

    a = debug.Timer("Pure python 4326<3395")
    for i in xrange(0, 100000):
        t = _c3395t4326(1, 2, 3072417.9458943508, 7020078.5326420991)
    a.stop()
    a = debug.Timer("TWMS wrapped 4326<3395")
    for i in xrange(0, 100000):
        t = to4326((3072417.9458943508, 7020078.5326420991), "EPSG:3395")
    a.stop()
    a = debug.Timer("Pyproj unwrapped 4326<3395")
    pr1 = projs["EPSG:3395"]["proj"]
    pr2 = projs["EPSG:4326"]["proj"]
    for i in xrange(0, 100000):
        t = pyproj.transform(pr1, pr2, 3072417.9458943508, 7020078.5326420991)
    a.stop()

    a = debug.Timer("Pure python 4326>3857")
    for i in xrange(0, 100000):
        t = _c4326t3857(1, 2, 27.6, 53.2)
    a.stop()
    a = debug.Timer("TWMS wrapped 4326>3857")
    for i in xrange(0, 100000):
        t = from4326((27.6, 53.2), "EPSG:3857")
    a.stop()
    a = debug.Timer("Pyproj unwrapped 4326>3857")
    pr2 = projs["EPSG:3857"]["proj"]

    pr1 = projs["EPSG:4326"]["proj"]
    for i in xrange(0, 100000):
        t = pyproj.transform(pr1, pr2, 27.6, 53.2)
    a.stop()

    a = debug.Timer("Pure python 4326>3395")
    for i in xrange(0, 100000):
        t = _c4326t3395(1, 2, 27.6, 53.2)
    a.stop()
    a = debug.Timer("TWMS wrapped 4326>3395")
    for i in xrange(0, 100000):
        t = from4326((27.6, 53.2), "EPSG:3395")
    a.stop()
    a = debug.Timer("Pyproj unwrapped 4326>3395")
    pr2 = projs["EPSG:3395"]["proj"]
    pr1 = projs["EPSG:4326"]["proj"]
    for i in xrange(0, 100000):
        t = pyproj.transform(pr1, pr2, 27.6, 53.2)
    a.stop()

    pass
