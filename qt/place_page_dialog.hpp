#pragma once

#include "search/reverse_geocoder.hpp"

#include <QtWidgets/QDialog>

#include <QtDebug>

namespace place_page
{
class Info;
}

class PlacePageDialog : public QDialog
{
  Q_OBJECT
public:
  PlacePageDialog(QWidget * parent, place_page::Info const & info,
                  search::ReverseGeocoder::Address const & address);

signals:
  void EditSignal();
  void CloseSignal();

private:
  void closeEvent(QCloseEvent * e) override;
};
