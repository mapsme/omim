import os
import json
import web
import tempfile
import shutil
import Image, StringIO

from mwm_rpc_server import MapsWithMeServer
MapsWithMe = MapsWithMeServer()

urls = (
    '/mwm/(.*?)/(.*?)/(.*?)/(.*?)\.png', 'renderhandler',
    '/compile_style', 'compile_style',
    '/(.*)', 'mainhandler'
)

class renderhandler:
    def GET(self, lang, z, x, y):
        img = MapsWithMe.RenderTileAsPng(z, x, y, language = lang, maxscale=False)
        ret_img_str = StringIO.StringIO()
        im = Image.open(StringIO.StringIO(img))
       # im = im.convert("L").convert("RGBA")
        im.save(ret_img_str, "PNG")
        return ret_img_str.getvalue()

class compile_style:
    def POST(self):
        stylesheet_file = tempfile.NamedTemporaryFile()
        stylesheet_file.write(web.input()["mapcss"])
        stylesheet_file.flush()
        os.system("pypy /home/kom/proj/kothic/src/komap.py -r mapswithme -s %s -t 19" % stylesheet_file.name)
        os.system("/home/kom/work/build-omim-release/out/release/generator_tool -generate_classif")
        shutil.copy2("/home/kom/work/omim/data/drules_proto.txt", "/home/kom/.local/share/MapsWithMe/")
        shutil.copy2("/home/kom/work/omim/data/drules_proto.bin", "/home/kom/.local/share/MapsWithMe/")
        MapsWithMe.Reload()
        return "ok"

class mainhandler:
    def GET(self, cc):
        return 'aa'

if __name__ == "__main__":
    app = web.application(urls, globals())
    app.run()                                                    # standalone run


application = web.application(urls, globals()).wsgifunc()        # mod_wsgi

#open('tile1.png', 'wb').write(RenderMapsWithMe('xhdpi').get_tile(18,151191,84322,512,512))
#open('tile2.png', 'wb').write(RenderMapsWithMe('mdpi').get_tile(18,151191,84322,256,256))