#! /usr/bin/env python2.7

from __future__ import print_function
from os.path import join
from sys import argv
import xml.etree.ElementTree as ET
from xml.dom import minidom

BEARING_TRANSFORMATION = 8 # 1 / ((360/256) / (360/32))

form_of_way = [
    "UNDEFINED",
    "MOTORWAY",
    "MULTIPLE_CARRIAGEWAY",
    "SINGLE_CARRIAGEWAY",
    "ROUNDABOUT",
    "TRAFFICSQUARE",
    "SLIPROAD",
    "OTHER"
]

namespaces = dict(
)


def append_not_empty(el_to, el_what):
    if el_what is None or el_to is None:
        return el_to
    if len(el_what) == 0 and el_what.text is None:
        return el_to
    el_to.append(el_what)
    return el_to


def el_with_text(tag, text):
    el = ET.Element(tag)
    el.text = text
    return el


def xpath_to_el(source_el, xpath, dest_tag, attr=None, transform=None):
    if not source_el is not None:
        return None
    temp_el = source_el.find(xpath, namespaces)
    if temp_el is None:
        return None

    text = temp_el.attrib[attr] if attr else temp_el.text

    if transform:
        text = transform(text)

    return el_with_text(dest_tag, text)


def namespaced(string):
    if ":" not in string:
        return string
    ns, tag = string.split(":", 1)
    return "{{{0}}}{1}".format(namespaces[ns], tag)


class OpenLRConverter:
    def __init__(self):
        self.lat = 0
        self.lon = 0

        self.last_lat = 0
        self.last_lon = 0

        self.last_point = None


    def generate_last_coords(self):
        coords = ET.Element("Coordinates")
        coords.append(el_with_text("Longitude", self.last_lon))
        coords.append(el_with_text("Latitude", self.last_lat))
        return coords


    def generate_last_point_once(self):
        if self.last_point is None:
            return None
        self.lat = self.last_lat
        self.lon = self.last_lon
        coords = self.generate_coords_element()
        ret = self.last_point
        ret.insert(0, coords)
        self.last_point = None
        return ret


    def generate_coords_element(self):
        coords = ET.Element("Coordinates")
        append_not_empty(coords, el_with_text("Longitude", str(self.lon)))
        append_not_empty(coords, el_with_text("Latitude", str(self.lat)))
        return coords


    def geneate_coords(self, element):
        source_coords = element.find("olr:coordinate", namespaces)
        lat = int(source_coords.find("olr:latitude", namespaces).text)
        lon = int(source_coords.find("olr:longitude", namespaces).text)

        if element.tag == namespaced("olr:first"):
            self.lat = lat
            self.lon = lon
            self.last_lat = lat
            self.last_lon = lon
            return self.generate_coords_element()

        if element.tag == namespaced("olr:last"):
            self.last_lat += lat
            self.last_lon += lon
            return None

        self.lat += lat
        self.lon += lon
        self.last_lat += lat
        self.last_lon += lon
        return self.generate_coords_element()


    def generate_line_attributes(self, element):
        line_attributes = ET.Element("LineAttributes")
        line_properties = element.find("olr:lineProperties", namespaces)

        append_not_empty(
            line_attributes,
            xpath_to_el(
                line_properties, "olr:frc",  "FRC", namespaced("olr:code"),
                transform=lambda x: "FRC" + x
            )
        )

        append_not_empty(
            line_attributes,
            xpath_to_el(
                line_properties, "olr:fow", "FOW", namespaced("olr:code"),
                transform=lambda x: form_of_way[int(x)]
            )
        )

        append_not_empty(
            line_attributes,
            xpath_to_el(
                line_properties, "olr:bearing/olr:value", "BEAR",
                transform=lambda x: str(int(x) / BEARING_TRANSFORMATION)
            )
        )
        return line_attributes


    def generate_path_properties(self, element):
        ret = ET.Element("PathAttributes")
        path_props = element.find("olr:pathProperties", namespaces)
        append_not_empty(
            ret, xpath_to_el(
                path_props, "olr:lfrcnp", "LFRCNP", namespaced("olr:code"),
                transform=lambda x: "FRC" + x
            )
        )
        append_not_empty(
            ret,
            xpath_to_el(path_props, "olr:dnp/olr:value", "DNP")
        )
        return ret


    def generate_point(self, element):
        el = ET.Element("LocationReferencePoint")
        append_not_empty(el, self.geneate_coords(element))
        append_not_empty(el, self.generate_line_attributes(element))
        append_not_empty(el, self.generate_path_properties(element))
        if element.tag == namespaced("olr:last"):
            self.last_point = el
            return None
        return el


    def generate_offsets(self, element):
        ret = ET.Element("Offsets")
        append_not_empty(ret, xpath_to_el(element, "olr:positiveOffset/olr:value", "PosOff"))
        append_not_empty(ret, xpath_to_el(element, "olr:negativeOffset/olr:value", "NegOff"))
        return ret


    def convert_report_segment(self, element, id):
        references = element.findall(
            "dflt:ReportSegmentLRC/dflt:method/olr:locationReference/olr:optionLinearLocationReference",
            namespaces
        )
        root = ET.Element("OpenLR", xmlns="http://www.openlr.org/openlr")
        root.append(el_with_text("LocationID", str(id)))
        for f in references:
            el = ET.Element("LineLocationReference")
            for g in f:
                if (g.tag == namespaced("olr:first") or
                    g.tag == namespaced("olr:last") or
                    g.tag == namespaced("olr:intermediates")
                ):
                    append_not_empty(el, self.generate_point(g))
                else:
                    append_not_empty(el, self.generate_last_point_once())
                    append_not_empty(el, self.generate_offsets(g))
            #in case we don't have any tags after points (e.g. offsets).
            # Will add the last point only if we haven't yet added one,
            # because otherwise self.last_point is none
            append_not_empty(el, self.generate_last_point_once())
            ref = ET.Element("XMLLocationReference")
            append_not_empty(ref, el)
            append_not_empty(root, ref)
        return ET.tostring(root, "utf-8")


if __name__ == "__main__":
    if len(argv) < 3:
        print("""ERROR:
        Usage:
        {0} file_to_process output_folder [--pretty]""".format(argv[0]))
        exit(1)
    xml_path = argv[1]

    output_path = argv[2]
    pretty = True if len(argv) > 3 and "--pretty" in argv[3:] else False

    tree = ET.parse(xml_path)
    root = tree.getroot()

    all_segments = root.findall("dflt:Dictionary/dflt:Report/dflt:reportSegments", namespaces)
    segments_count = len(all_segments)
    i = 0

    for f in all_segments:
        seg_id = f.find(namespaced("dflt:ReportSegmentID")).text
        transformer = OpenLRConverter()

        res = transformer.convert_report_segment(f, seg_id)

        if pretty:
            preparsed = minidom.parseString(res)
            res = preparsed.toprettyxml(indent="    ")

        with open(join(argv[2], "{}.xml".format(seg_id)), "w") as outfile:
            outfile.write(res)
        i += 1
        print("Done {0} of {1}".format(i, segments_count))

    print("Done")
