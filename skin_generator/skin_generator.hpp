#pragma once

#include "geometry/rect2d.hpp"
#include "geometry/packer.hpp"

#include "coding/writer.hpp"

#include "base/base.hpp"

#include <vector>
#include <list>
#include <string>
#include <map>

#include <boost/gil/gil_all.hpp>

#include <QtGui/QPainter>
#include <QtGui/QImage>
#include <QtCore/QFileInfo>
#include <QtCore/QSize>
#include <QtSvg/QSvgRenderer>
#include <QtXml/QXmlContentHandler>
#include <QtXml/QXmlDefaultHandler>

class QImage;

namespace gil = boost::gil;

namespace tools
{
  class SkinGenerator
  {
  public:
      struct SymbolInfo
      {
        QSize m_size;
        QString m_fullFileName;
        QString m_symbolID;

        m2::Packer::handle_t m_handle;

        SymbolInfo() {}
        SymbolInfo(QSize size, QString const & fullFileName, QString const & symbolID)
          : m_size(size), m_fullFileName(fullFileName), m_symbolID(symbolID) {}
      };

      typedef std::vector<SymbolInfo> TSymbols;

      struct SkinPageInfo
      {
        TSymbols m_symbols;
        uint32_t m_width;
        uint32_t m_height;
        std::string m_fileName;
        std::string m_dir;
        std::string m_suffix;
        m2::Packer m_packer;
      };

  private:

      bool m_needColorCorrection;

      QSvgRenderer m_svgRenderer;

      typedef std::vector<SkinPageInfo> TSkinPages;
      TSkinPages m_pages;

      bool m_overflowDetected;
      void markOverflow();

  public:

      SkinGenerator(bool needColorCorrection);
      //void processFont(string const & fileName, string const & skinName, vector<int8_t> const & fontSizes, int symbolScale);
      void processSymbols(std::string const & symbolsDir,
                          std::string const & skinName,
                          std::vector<QSize> const & symbolSizes,
                          std::vector<std::string> const & suffix);
      bool renderPages(uint32_t maxSize);
      bool writeToFile(std::string const & skinName);
      void writeToFileNewStyle(std::string const & skinName);
    };
} // namespace tools
