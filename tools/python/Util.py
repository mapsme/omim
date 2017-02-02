from contextlib import contextmanager
import shutil
import tempfile
from os.path import abspath

try:
    from tempfile import TemporaryDirectory
except ImportError:
    @contextmanager
    def TemporaryDirectory():
        name = tempfile.mkdtemp()
        try:
            yield name
        finally:
            shutil.rmtree(name)


def find_omim():
    my_path = abspath(__file__)
    tools_index = my_path.rfind("/tools/python")
    omim_path = my_path[:tools_index]
    return omim_path
