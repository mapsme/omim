#pragma once

#include "generator/feature_merger.hpp"

#include "indexer/cell_id.hpp"

#include "geometry/tree4d.hpp"
#include "geometry/region2d.hpp"

#include "std/fstream.hpp"
#include "std/list.hpp"

class FeatureBuilder1;

class CoastlineFeaturesGenerator
{
  FeatureMergeProcessor m_merger;

  using TTree = m4::Tree<m2::RegionI>;
  TTree m_tree;

  ofstream m_dumpStream;
  list<vector<m2::PointD>> m_regions;

  uint32_t m_coastType;

public:
  CoastlineFeaturesGenerator(uint32_t coastType);

  void AddRegionToTree(FeatureBuilder1 const & fb);

  TreeT const & GetRegionTree() const { return m_tree; }
  size_t DumpCoastlines(string const & intermediateDir) const;

  void operator() (FeatureBuilder1 const & fb);
  /// @return false if coasts are not merged and FLAG_fail_on_coasts is set
  bool Finish(bool needStopIfFail, string const & intermediateDir);

  void GetFeatures(vector<FeatureBuilder1> & vecFb);
};
