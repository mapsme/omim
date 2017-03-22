#pragma once

#include "search/result.hpp"
#include "search/search_quality/assessment_tool/edits.hpp"
#include "search/search_quality/sample.hpp"

#include <QtWidgets/QWidget>

class LanguagesList;
class QLineEdit;
class ResultsView;

class SampleView : public QWidget, public Edits::Delegate
{
  Q_OBJECT

public:
  using Relevance = search::Sample::Result::Relevance;

  explicit SampleView(QWidget * parent);

  void SetContents(search::Sample const & sample);
  void ShowResults(search::Results::Iter begin, search::Results::Iter end);
  void SetResultRelevances(std::vector<Relevance> const & relevances);

  // Edits::Delegate overrides:
  void OnUpdate();

signals:
  void EditStateUpdated(bool hasEdits);

private:
  QLineEdit * m_query = nullptr;
  LanguagesList * m_langs = nullptr;
  ResultsView * m_results = nullptr;

  Edits m_edits;
};
