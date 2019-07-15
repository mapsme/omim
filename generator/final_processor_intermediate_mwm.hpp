#pragma once

#include "generator/coastlines_generator.hpp"
#include "generator/feature_generator.hpp"
#include "generator/world_map_generator.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace generator
{
enum class FinalProcessorPriority : uint8_t
{
  CountriesOrWorld = 1,
  WorldCoasts = 2
};

// Classes that inherit this interface implement the final stage of intermediate mwm processing.
// For example, attempt to merge the coastline or adding external elements.
// Each derived class has a priority. This is done to comply with the order of processing intermediate mwm,
// taking into account the dependencies between them. For example, before adding a coastline to
// a country, we must build coastline. Processors with higher priority will be called first.
// Processors with the same priority can run in parallel.
class FinalProcessorIntermediateMwmInterface
{
public:
  explicit FinalProcessorIntermediateMwmInterface(FinalProcessorPriority priority);
  virtual ~FinalProcessorIntermediateMwmInterface() = default;

  virtual bool Process() = 0;

  bool operator<(FinalProcessorIntermediateMwmInterface const & other) const;
  bool operator==(FinalProcessorIntermediateMwmInterface const & other) const;
  bool operator!=(FinalProcessorIntermediateMwmInterface const & other) const;

protected:
  FinalProcessorPriority m_priority;
};

class CountryFinalProcessor : public FinalProcessorIntermediateMwmInterface
{
public:
  explicit CountryFinalProcessor(std::string const & borderPath,
                                 std::string const & temporaryMwmPath,
                                 bool haveBordersForWholeWorld,
                                 size_t threadsCount);

  void SetBooking(std::string const & filename);
  void SetCitiesAreas(std::string const & filename);
  void SetPromoCatalog(std::string const & filename);
  void SetCoastlines(std::string const & coastlineGeomFilename,
                     std::string const & worldCoastsFilename);

  void DumpCitiesBoundaries(std::string const & filename);

  // FinalProcessorIntermediateMwmInterface overrides:
  bool Process() override;

private:
  bool ProcessBooking();
  bool ProcessCities();
  bool ProcessCoastline();
  bool Finish();

  std::string m_borderPath;
  std::string m_temporaryMwmPath;
  std::string m_citiesAreasTmpFilename;
  std::string m_citiesBoundariesFilename;
  std::string m_hotelsFilename;
  std::string m_coastlineGeomFilename;
  std::string m_worldCoastsFilename;
  std::string m_citiesFilename;
  bool m_haveBordersForWholeWorld;
  size_t m_threadsCount;
};

class WorldFinalProcessor : public FinalProcessorIntermediateMwmInterface
{
public:
  using WorldGenerator = WorldMapGenerator<feature::FeaturesCollector>;

  explicit WorldFinalProcessor(std::string const & temporaryMwmPath,
                               std::string const & coastlineGeomFilename);

  void SetPopularPlaces(std::string const & filename);
  void SetCitiesAreas(std::string const & filename);
  void SetPromoCatalog(std::string const & filename);

  // FinalProcessorIntermediateMwmInterface overrides:
  bool Process() override;

private:
  bool ProcessCities();

  std::string m_temporaryMwmPath;
  std::string m_worldTmpFilename;
  std::string m_coastlineGeomFilename;
  std::string m_popularPlacesFilename;
  std::string m_citiesAreasTmpFilename;
  std::string m_citiesFilename;
};

class CoastlineFinalProcessor : public FinalProcessorIntermediateMwmInterface
{
public:
  explicit CoastlineFinalProcessor(std::string const & filename);

  void SetCoastlinesFilenames(std::string const & geomFilename, std::string const & rawGeomFilename);

  // FinalProcessorIntermediateMwmInterface overrides:
  bool Process() override;

private:
  std::string m_filename;
  std::string m_coastlineGeomFilename;
  std::string m_coastlineRawGeomFilename;
  CoastlineFeaturesGenerator m_generator;
};
}  // namespace generator
