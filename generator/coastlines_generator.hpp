#pragma once

#include "generator/feature_merger.hpp"

#include "std/list.hpp"

class FeatureBuilder1;

class CoastlineFeaturesGenerator
{
  FeatureMergeProcessor m_merger;

  list<vector<m2::PointD>> m_regions;

  uint32_t m_coastType;
public:
  CoastlineFeaturesGenerator(uint32_t coastType);

  void AddRegion(FeatureBuilder1 const & fb);

  void operator() (FeatureBuilder1 const & fb);
  bool Finish(string const & intermediateDir);

  bool MakePolygons(list<FeatureBuilder1> & vecFb);
};
