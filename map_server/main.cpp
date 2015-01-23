#include "map_server/main.hpp"

#include "indexer/mercator.hpp"

#include "map/framework.hpp"

#include "platform/platform.hpp"

#include "std/shared_ptr.hpp"

#include <qjsonrpcservice.h>

#include "3party/gflags/src/gflags/gflags.h"

#include <QtOpenGL/QGLPixelBuffer>
#include <QtCore/QBuffer>
#include <QtCore/QFile>

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
  #include <QtGui/QApplication>
#else
  #include <QtWidgets/QApplication>
#endif


DEFINE_uint64(texture_size, 2560, "Texture size");
DEFINE_string(listen, "/tmp/mwm-render-socket",
                 "Path to socket to be listened");

MwmRpcService::MwmRpcService(QObject * parent) : m_pixelBuffer(new QGLPixelBuffer(FLAGS_texture_size, FLAGS_texture_size))
{
  LOG(LINFO, ("MwmRpcService started"));

  m_pixelBuffer->makeCurrent();
}

MwmRpcService::~MwmRpcService()
{
  m_framework.PrepareToShutdown();
}

QString MwmRpcService::RenderBox(
    QVariant const & bbox,
    int width,
    int height,
    QString const & density,
    QString const & language,
    bool maxScaleMode
    )
{
  LOG(LINFO, ("Render box started", width, height, maxScaleMode));
  return QString();
}

bool MwmRpcService::Ping()
{
  return true;
}

void MwmRpcService::Exit()
{
  qApp->exit(0);
}

int main(int argc, char *argv[])
{
  google::SetUsageMessage("Usage: MapsWithMe-server [-listen SOCKET]");
  google::ParseCommandLineFlags(&argc, &argv, true);

  QApplication app(argc, argv);

  QString socketPath(FLAGS_listen.c_str());

  if (QFile::exists(socketPath))
  {
    if (!QFile::remove(socketPath))
    {
      qDebug() << "couldn't delete temporary service";
      return -1;
    }
  }

  MwmRpcService service;
  QJsonRpcLocalServer rpcServer;
  rpcServer.addService(&service);
  if (!rpcServer.listen(socketPath))
  {
    qDebug() << "could not start server: " << rpcServer.errorString();
    return -1;
  }

  return app.exec();
}
