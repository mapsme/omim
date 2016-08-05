from __future__ import print_function

import logging
import re
import subprocess
from argparse import ArgumentParser
from collections import defaultdict
from itertools import chain

from find_untranslated_strings import StringsTxt


MACRO_RE =  re.compile('L\(.*?@\"(.*?)\"\)')
XML_RE = re.compile("value=\"(.*?)\"")
ANDROID_JAVA_RE = re.compile("R\.string\.([\w_]*)")
ANDROID_XML_RE = re.compile("@string/(.*?)\W")
IOS_CANDIDATES_RE = re.compile("[^L\(]@\"([a-z0-9_]*?)\"")


HARDCODED_CATEGORIES = [
    "food", "hotel", "tourism", "wifi", "transport", "fuel", "parking", "shop",
    "atm", "bank", "entertainment", "hospital", "pharmacy", "police", "toilet", "post"
]

HARDCODED_COLOURS = [
    "red", "yellow", "blue", "green", "purple", "orange", "brown", "pink"
]


def exec_shell(test, flags):
    spell = ["{0} {1}".format(test, flags)]

    process = subprocess.Popen(
        spell,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        shell=True
    )

    logging.info(" ".join(spell))
    out, _ = process.communicate()
    return filter(None, out.splitlines())


def grep_ios():
    logging.info("Grepping ios")
    grep = "grep -r 'L(\|localizedText\|localizedPlaceholder' ../../iphone/*"
    ret = exec_shell(grep, "")
    return filter_ios_grep(ret)


def grep_android():
    logging.info("Grepping android")
    grep = "grep -r 'R.string.' ../../android/src"
    ret = android_grep_wrapper(grep, ANDROID_JAVA_RE)
    grep = "grep -r '@string/' ../../android/res"
    ret.update(android_grep_wrapper(grep, ANDROID_XML_RE))
    grep = "grep -r '@string/' ../../android/AndroidManifest.xml"
    ret.update(android_grep_wrapper(grep, ANDROID_XML_RE))

    return parenthesize(ret)


def grep_ios_candidates():
    logging.info("Grepping ios candidates")
    grep = "grep -r '@\"' ../../iphone/*"
    ret = exec_shell(grep, "")
    return parenthesize(strings_from_grepped(ret, IOS_CANDIDATES_RE))


def android_grep_wrapper(grep, regex):
    grepped = exec_shell(grep, "")
    return strings_from_grepped(grepped, regex)


def filter_ios_grep(strings):
    with_no_blobs = filter(lambda s: not s.startswith("Binary file"), strings)
    filtered = strings_from_grepped(with_no_blobs, MACRO_RE)
    filtered = parenthesize(process_ternary_operators(filtered))
    filtered.update(parenthesize(strings_from_grepped(with_no_blobs, XML_RE)))
    filtered.update(parenthesize(HARDCODED_CATEGORIES))
    filtered.update(parenthesize(HARDCODED_COLOURS))
    return filtered


def process_ternary_operators(filtered):
    return chain(*map(lambda s: s.split('" : @"'), filtered))


def strings_from_grepped(grepped, regexp):
    return set(
        chain(
            *filter(
                None, map(lambda s: regexp.findall(s), grepped)
            )
        )
    )


def parenthesize(strings):
    return set(map(lambda s: "[{0}]".format(s), strings))


def write_filtered_strings_txt(filtered, filepath, languages=None):
    logging.info("Writing strings to file {0}".format(filepath))
    strings_txt = StringsTxt()
    strings_dict = {key : dict(strings_txt.translations[key]) for key in filtered}
    strings_txt.translations = strings_dict
    strings_txt.comments_and_tags = {}
    strings_txt.write_formatted(filepath, languages=languages)


def get_args():
    parser = ArgumentParser(
        description="A script for cleaning up the strings.txt file. It can cleanup the file inplace, that is all the unused strings will be removed from strings.txt, or it can produce two separate files for ios and android. We can also produce the compiled string resources specifically for each platform that do not contain strings for other platforms or comments.")

    parser.add_argument(
        "-s", "--single-file",
        dest="single", default=False,
        action="store_true",
        help="Create single cleaned up file for both platform. All strings that are not used in the project will be thrown away."
    )

    parser.add_argument(
        "-l", "--language",
        dest="langs", default=None,
        action="append",
        help="The languages to be included into the resulting strings.txt file or files. If this param is empty, all languages from the current strings.txt file will be preserved."
    )

    parser.add_argument(
        "-g", "--generate-localizations",
        dest="generate", default=False,
        action="store_true",
        help="Generate localizations for the platforms."
    )

    parser.add_argument(
        "-o", "--output",
        dest="output", default="strings.txt",
        help="The name for the resulting file. It will be saved to the project folder. Only relevant if the -s option is set."
    )

    parser.add_argument(
        "-m", "--missing-strings",
        dest="missing", default=False,
        action="store_true",
        help="Find the keys that are used in iOS, but are not translated in strings.txt and exit."
    )

    parser.add_argument(
        "-c", "--candidates",
        dest="candidates", default=False,
        action="store_true",
        help="Find the strings in iOS that are not in the L() macros, but that look like they might be keys."
    )



    return parser.parse_args()


def do_multiple(args):
    write_filtered_strings_txt(grep_ios(), "../../ios_strings.txt", args.langs)
    write_filtered_strings_txt(grep_android(), "../../android_strings.txt", args.langs)
    if args.generate:
        exec_shell(
            "../unix/generate_localizations.sh", "android_strings.txt ios_strings.txt"
        )


def generate_auto_tags(ios, android):
    new_tags = defaultdict(list)
    for i in ios:
        new_tags[i].append("ios")

    for a in android:
        new_tags[a].append("android")

    for key in new_tags:
        new_tags[key] = ", ".join(new_tags[key])

    return new_tags


def new_comments_and_tags(strings_txt, filtered, new_tags):
    comments_and_tags = {key: strings_txt.comments_and_tags[key] for key in filtered}
    for key in comments_and_tags:
        comments_and_tags[key]["tags"] = new_tags[key]
    return comments_and_tags


def do_single(args):
    ios = grep_ios()
    android = grep_android()

    new_tags = generate_auto_tags(ios, android)

    filtered = ios
    filtered.update(android)

    strings_txt = StringsTxt()
    strings_txt.translations = {key: dict(strings_txt.translations[key]) for key in filtered}

    strings_txt.comments_and_tags = new_comments_and_tags(strings_txt, filtered, new_tags)

    strings_txt.write_formatted(languages=args.langs, target_file="../../{0}".format(args.output))

    if args.generate:
        exec_shell(
            "../unix/generate_localizations.sh", "{0} {0}".format(args.output)
        )

def do_missing(args):
    ios = set(grep_ios())
    strings_txt_keys = set(StringsTxt().translations.keys())
    missing = ios - strings_txt_keys

    if missing:
        for m in missing:
            logging.info(m)
        exit(1)
    else:
        logging.info("Ok. No missing strings.")
        exit(0)


def do_candidates(args):
    all_candidates = set(grep_ios_candidates())
    strings_txt_keys = set(StringsTxt().translations.keys())
    candidates_in_txt = all_candidates & strings_txt_keys
    properly_used = grep_ios()

    improperly_used_candidates = candidates_in_txt - properly_used

    for s in improperly_used_candidates:
        print(s)


    # for a in all_candidates:
    #     print(a)
    #
    # pass


def do_ios_suspects(args):
    grep = "grep -re 'L(' ../../iphone/*"
    suspects = exec_shell(grep, "")
    SUSPECT_RE = re.compile(r"(.*?):.*?\WL\(([^@].*?)\)")
    strings = strings_from_grepped(suspects, SUSPECT_RE)
    for s in strings:
        print(s)




if __name__ == "__main__":



    logging.basicConfig(level=logging.DEBUG)
    args = get_args()
    args.langs = set(args.langs) if args.langs is not None else None

    # do_ios_suspects(args)
    do_candidates(args)
    exit(4)



    if args.missing:
        do_missing(args)

    if args.candidates:
        do_candidates(args)
        exit(2)

    if not args.single:
        do_multiple(args)
        exit(0)

    do_single(args)
