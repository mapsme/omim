#pragma once

#include "../geometry/rect2d.hpp"

#include "../coding/file_writer.hpp"

#include "../std/vector.hpp"
#include "../std/string.hpp"


class FeatureBuilder1;

namespace feature
{
  struct GenerateInfo;

  bool GenerateFeatures(GenerateInfo & info, bool lightNodes);

  /// Base collector - writes features to the file.
  class FeaturesCollector
  {
  protected:
    FileWriter m_datFile;

    m2::RectD m_bounds;

  protected:
    static uint32_t GetFileSize(FileWriter const & f);

    void WriteFeatureBase(vector<char> const & bytes, FeatureBuilder1 const & fb);

  public:
    FeaturesCollector(string const & fName);

    void operator() (FeatureBuilder1 const & f);
  };

  /// Collect strings from features and save most used of them to the file.
  /// FeaturesCollector2 will use them later for better coding.
  class FeaturesCollector1 : public FeaturesCollector
  {
    typedef FeaturesCollector BaseT;

    map<string, uint32_t> m_strings;

  public:
    FeaturesCollector1(string const & fName) : BaseT(fName) {}
    ~FeaturesCollector1();

    void operator() (FeatureBuilder1 const & f);
    bool operator() (char lang, string const & s);
  };
}
