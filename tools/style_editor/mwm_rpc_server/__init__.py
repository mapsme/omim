import jsonrpc
import socket
import base64
import os
from threading import Lock
from tempfile import NamedTemporaryFile
import projections
import time
import subprocess

class MapsWithMeServer:
    class MwmInstance:
        socket = "/tmp/mwm-render-socket"
        socket_file = False
        rpc = False
        lock = Lock()
        process = False

        def __init__(self):
            self.socket_file = NamedTemporaryFile()
            self.socket = self.socket_file.name
            self.rpc = jsonrpc.ServerProxy(
                jsonrpc.JsonRpc20(
                ),
                jsonrpc.TransportSocket(
                    addr=self.socket,
                    sock_type=socket.AF_UNIX,
                    timeout=20))
            self.reconnect()

        def reconnect(self):
            try:
                self.rpc.MapsWithMe.Ping()
            except jsonrpc.RPCTransportError:
                self.lock.acquire()
                if self.process:
                    self.process.kill()
                self.process = subprocess.Popen(
                    ["MapsWithMe-server", "--listen=%s" % (self.socket), "--texture_size=512"], stdout=open(os.devnull,'w'), stderr=open(os.devnull,'w'))
                while True:
                    try:
                        if self.rpc.MapsWithMe.Ping():
                            break
                    except (jsonrpc.RPCTransportError, jsonrpc.RPCParseError):
                        time.sleep(0.1)
                self.lock.release()

        def __del__(self):
            try:
                self.process.kill()
            except:
                pass

    free_instances = []

    def __init__(self):
        self.Reload()

    def acquire_instance(self):
      attempts = 0
      while attempts < 10:
        try:
            return self.free_instances.pop()
        except:
            attempts += 1
            time.sleep(1)
      return self.MwmInstance()

    def release_instance(self, instance):
        self.free_instances.append(instance)

    def RenderBoxAsPng(self, bbox, size,
                       dpi="mdpi", language="ru", maxscale=False):
        (width, height) = size
        mwm = self.acquire_instance()
        try:
            a = base64.decodestring(
                mwm.rpc.MapsWithMe.RenderBox(bbox,
                                              int(width),
                                              int(height),
                                              dpi,
                                              language,
                                              maxscale))
        except (jsonrpc.RPCTransportError, jsonrpc.RPCParseError):
            mwm.reconnect()
            return
        finally:
            self.release_instance(mwm)
        return a

    def RenderTileAsPng(self, z, x, y, dpi="xhdpi",
                        size=512, language="ru", maxscale=False):
        return self.RenderBoxAsPng(projections.bbox_by_tile(int(z) + 1, int(x), int(y)), (int(size), int(size)), dpi, language, maxscale)

    def Reload(self):
        self.free_instances = []
        for x in xrange(4):
            self.release_instance(self.MwmInstance())


if __name__ == "__main__":
   # print "Starting"
    MapsWithMe = MapsWithMeServer()
    print "Rendering tile 4"
    print >> open('xhdpi.png', 'w'), MapsWithMe.RenderTileAsPng(0, 0, 0, "xhdpi")
    print "Rendering tile 1"
    print >> open('ldpi.png', 'w'), MapsWithMe.RenderTileAsPng(0, 0, 0, "ldpi")
    print "Rendering tile 2"
    print >> open('mdpi.png', 'w'), MapsWithMe.RenderTileAsPng(0, 0, 0, "mdpi")
    print "Rendering tile 4"
    print >> open('xhdpi.png', 'w'), MapsWithMe.RenderTileAsPng(0, 0, 0, "xhdpi")
    # print "Rendering tile 3"
    #import random
    #dpi = random.choice(["ldpi", "mdpi", "hdpi", "xhdpi"])
   # print >> open(dpi + '.png', 'w'), MapsWithMe.RenderTileAsPng(0, 0, 0, dpi)
