from __future__ import print_function

from collections import defaultdict


#msgid
#msgstr
class PoParser:
    def __init__(self, filepath):
        self.translations = defaultdict(str)
        current_key = None
        string_started = False
        with open(filepath) as infile:
            for line in infile:
                if line.startswith("msgid"):
                    current_key = self.clean_line(line,"msgid")
                elif line.startswith("msgstr"):
                    if not current_key:
                        continue
                    translation = self.clean_line(line, "msgstr")
                    if not translation:
                        print("No translation for key {}".format(current_key))
                        continue
                    self.translations[current_key] = translation
                    string_started = True
                elif not line or line.startswith("#"):
                    string_started = False
                    current_key = None
                else:
                    if not string_started:
                        continue
                    self.translations[current_key] = "{}{}".format(self.translations[current_key], self.clean_line(line))




    def clean_line(self, line, prefix=""):
        return line[len(prefix):].strip().strip('"')


def main():
    parser = PoParser("en.po", "en")
    for key, tr in parser.translations.iteritems():
        print("  [{}]\n    en = {}".format(key, tr))
    pass

if __name__ == "__main__":
    main()