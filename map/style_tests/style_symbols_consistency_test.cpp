#include "testing/testing.hpp"

#include "helpers.hpp"

#include "indexer/classificator_loader.hpp"
#include "indexer/drawing_rules.hpp"
#include "indexer/drules_include.hpp"
#include "indexer/drules_struct.pb.h"
#include "indexer/map_style_reader.hpp"

#include "base/logging.hpp"
#include "base/string_utils.hpp"

#include "coding/parse_xml.hpp"
#include "coding/reader.hpp"

#include "std/algorithm.hpp"
#include "std/set.hpp"
#include "std/string.hpp"
#include "std/transform_iterator.hpp"
#include "std/vector.hpp"

namespace
{
class SdfParsingDispatcher
{
public:
  SdfParsingDispatcher(set<string> & symbols) : m_symbols(symbols) {}
  bool Push(string const &) { return true; }
  void Pop(string const &) {}
  void CharData(string const &) {}
  void AddAttr(string const & attribute, string const & value)
  {
    if (attribute == "name")
      m_symbols.insert(value);
  }

private:
  set<string> & m_symbols;
};

set<string> GetSymbolsSetFromDrawingRule()
{
  set<string> symbols;
  drule::rules().ForEachRule([&symbols](int, int, int, drule::BaseRule const * rule) {
    SymbolRuleProto const * const symbol = rule->GetSymbol();
    if (nullptr != symbol && symbol->has_name())
      symbols.insert(symbol->name());
  });
  return symbols;
}

set<string> GetSymbolsSetFromResourcesFile(string const & density)
{
  set<string> symbols;
  SdfParsingDispatcher dispatcher(symbols);
  ReaderPtr<Reader> reader = GetStyleReader().GetResourceReader("symbols.sdf", density);
  ReaderSource<ReaderPtr<Reader>> source(reader);
  ParseXML(source, dispatcher);
  return symbols;
}

// ClassifName -> array of [Scale, SymbolName]
// in case of 'apply_if' style, array may contain more than 1 element
using SymbolsMapping = map<string, vector<pair<int, string>>>;

SymbolsMapping GetCurrentStyleSymbolsMapping()
{
  SymbolsMapping mapping;

  string drawingRules;
  GetStyleReader().GetDrawingRulesReader().ReadAsString(drawingRules);

  ContainerProto cont;
  cont.ParseFromString(drawingRules);
  for (int ci = 0; ci < cont.cont_size(); ++ci)
  {
    ClassifElementProto const & classifElement = cont.cont(ci);
    for (int ei = 0; ei < classifElement.element_size(); ++ei)
    {
      DrawElementProto const & drawElement = classifElement.element(ei);
      if (drawElement.has_symbol())
      {
        mapping[classifElement.name()].emplace_back(drawElement.scale(),
                                                    drawElement.symbol().name());
      }
    }
  }

  return mapping;
}

// Returns base name of icon.
// According to agreement icon name consists of 'basename-suffix'
// where suffix says about icon size, while basename identifies icon,
// suffix is optional.
string GetIconBaseName(string const & name) { return name.substr(0, name.find_last_of('-')); }
}  // namespace

UNIT_TEST(Test_SymbolsConsistency)
{
  // Tests that all symbols specified in drawing rules have corresponding symbols in resources

  bool res = true;

  string const densities[] = {"mdpi", "hdpi", "xhdpi", "xxhdpi", "6plus"};

  styles::RunForEveryMapStyle([&](MapStyle mapStyle) {
    set<string> const drawingRuleSymbols = GetSymbolsSetFromDrawingRule();

    for (string const & density : densities)
    {
      set<string> const resourceStyles = GetSymbolsSetFromResourcesFile(density);

      vector<string> missed;
      set_difference(drawingRuleSymbols.begin(), drawingRuleSymbols.end(), resourceStyles.begin(),
                     resourceStyles.end(), back_inserter(missed));

      if (!missed.empty())
      {
        // We are interested in all set of bugs, therefore we do not stop test here but
        // continue it just keeping in res that test failed.
        LOG(LINFO, ("Symbols mismatch: style", mapStyle, ", density", density, ", missed", missed));
        res = false;
      }
    }
  });

  TEST(res, ());
}

UNIT_TEST(Test_ClassSymbolsConsistency)
{
  // Tests that symbols specified for object class are same for all zoom levels,
  // e.g. lawyer object class has 'lawyer' icon for all zoom levels.
  // Also test checks that visible scales are consecutive - without holes:
  // e.g. 3,4,5 - ok, but 4,5,7 - not ok (6 is missing).

  bool success = true;
  styles::RunForEveryMapStyle([&success](MapStyle mapStyle) {
    for (auto const & classifMapping : GetCurrentStyleSymbolsMapping())
    {
      auto const & scaleAndSymbolMapping = classifMapping.second;
      if (scaleAndSymbolMapping.empty())
        continue;

      bool invalidSymbol = false;
      bool invalidScale = false;

      auto prev = *scaleAndSymbolMapping.begin();
      for (auto itr = ++scaleAndSymbolMapping.begin(); itr != scaleAndSymbolMapping.end(); ++itr)
      {
        auto const curr = *itr;
        if (GetIconBaseName(prev.second) != GetIconBaseName(curr.second))
          invalidSymbol = true;
        if ((prev.first != curr.first) && ((prev.first + 1) != curr.first))
          invalidScale = true;
        prev = curr;
      }

      // We are interested in all bugs, therefore we do not stop test but
      // continue it just keeping in 'res' that test failed.
      if (invalidSymbol)
      {
        auto const fn = [](pair<const int, string> const & p) { return p.second; };
        string const icons =
            strings::JoinStrings(make_transform_iterator(scaleAndSymbolMapping.cbegin(), fn),
                                 make_transform_iterator(scaleAndSymbolMapping.cend(), fn), ", ");
        LOG(LINFO, ("Class symbol icons mismatch: style:", mapStyle, ", class:",
                    classifMapping.first, ", icons:", icons));
      }
      if (invalidScale)
      {
        auto const fn = [](pair<const int, string> const & p) {
          return strings::to_string(p.first);
        };
        string const levels =
            strings::JoinStrings(make_transform_iterator(scaleAndSymbolMapping.cbegin(), fn),
                                 make_transform_iterator(scaleAndSymbolMapping.cend(), fn), ", ");
        LOG(LINFO, ("Class symbol zoom order mismatch: style:", mapStyle, ", class:",
                    classifMapping.first, ", levels:", levels));
      }

      if (invalidScale || invalidSymbol)
        success = false;
    }
  });

  TEST(success, ());
}
