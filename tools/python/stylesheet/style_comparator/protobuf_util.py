from google.protobuf.descriptor import FieldDescriptor
from operator import attrgetter
import drules_struct_pb2 as proto


def make_key(element, field_name=None):
    key = ""
    if field_name is not None:
        key = field_name + "/"

    keys_and_formats = [
        ("name", "/{}"),
        ("scale", "/<{}>"),
        ("priority", "/__{}"),
        ("apply_if", "/_?_{}")
    ]

    for key_part, fmt in keys_and_formats:
        key += make_key_part(element, key_part, fmt)

    key += relevant_class(element)
    return key + "/"


def relevant_class(element):
    class_name = type(element).__name__
    if class_name.endswith("RuleProto"):
        return "/{{{}}}".format(class_name)
    return ""


def make_key_part(element, field, formatr):
    key_part = getattr(element, field, None)
    if key_part or key_part == 0:
        return formatr.format(key_part)
    return ""


def list_message_fields(sub_drules):
    if isinstance(sub_drules, list):
        return [(make_key(f), f) for f in sub_drules]

    return [
        (
            make_key(field[1], field_name=field[0].name),
            list(field[1])
            if field[0].label == FieldDescriptor.LABEL_REPEATED
            else field[1]
        )
        for field in sub_drules.ListFields()
        if field[0].message_type is not None
    ]


def list_leaves_fields(sub_drules):
    try:
        return [
            x[0]
            for x in sub_drules.ListFields()
            if x[0].message_type is None
        ]
    except AttributeError:  # subdrules may be a list or the root element
        return []


def has_color_fields(sub_drules):
    return bool(color_fields(sub_drules))


def non_color_fields(sub_drules):
    return map(
        attrgetter("name"),
            filter(
            lambda s: not s.name.endswith("color") and s.name != "priority",
            list_leaves_fields(sub_drules)
        )
    )


def color_fields(sub_drules):
    field_names = map(attrgetter("name"), list_leaves_fields(sub_drules))
    # We rely on the convention that all the color field names end in "color".
    color_field_names = filter(lambda x: x.endswith("color"), field_names)
    # Here we filter on HasField because an element can have optional fields,
    # and when we try to access their fields, they will return their default values.
    return filter(lambda x: sub_drules.HasField(x), color_field_names)


def read_bin(filepath):
    drules = proto.ContainerProto()
    with open(filepath) as infile:
        drules.ParseFromString(infile.read())
    return drules
