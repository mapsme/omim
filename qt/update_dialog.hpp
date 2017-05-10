#pragma once

#include "map/framework.hpp"

#include <unordered_map>
#include <vector>

#include <QtWidgets/QApplication>
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
  #include <QtGui/QDialog>
#else
  #include <QtWidgets/QDialog>
#endif

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QPushButton;

class Framework;

namespace qt
{
  class UpdateDialog : public QDialog
  {
    Q_OBJECT

  public:
    explicit UpdateDialog(QWidget * parent, Framework & framework);
    virtual ~UpdateDialog();

    /// @name Called from downloader to notify GUI
    //@{
    void OnCountryChanged(storage::TCountryId const & countryId);
    void OnCountryDownloadProgress(storage::TCountryId const & countryId,
                                   storage::MapFilesDownloader::TProgress const & progress);
    //@}

    void ShowModal();

  private slots:
    void OnItemClick(QTreeWidgetItem * item, int column);
    void OnCloseClick();
    void OnTextChanged(QString const & text);

  private:
    void FillTree(std::string const & filter);
    void FillTreeImpl(QTreeWidgetItem * parent, storage::TCountryId const & countryId, std::string const & filter);
    void UpdateRowWithCountryInfo(storage::TCountryId const & countryId);
    void UpdateRowWithCountryInfo(QTreeWidgetItem * item, storage::TCountryId const & countryId);
    QString GetNodeName(storage::TCountryId const & countryId);

    QTreeWidgetItem * CreateTreeItem(storage::TCountryId const & countryId, QTreeWidgetItem * parent);
    std::vector<QTreeWidgetItem *> GetTreeItemsByCountryId(storage::TCountryId const & countryId);
    storage::TCountryId GetCountryIdByTreeItem(QTreeWidgetItem *);

  private:
    inline storage::Storage & GetStorage() const { return m_framework.GetStorage(); }

    QTreeWidget * m_tree;
    Framework & m_framework;
    int m_observerSlotId;

    std::unordered_multimap<storage::TCountryId, QTreeWidgetItem *> m_treeItemByCountryId;
  };
} // namespace qt
