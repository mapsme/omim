#pragma once

#include "generator/coastlines_generator.hpp"
#include "generator/feature_generator.hpp"
#include "generator/world_map_generator.hpp"

#include <string>

namespace generator
{
enum class FinalProcessorPriority : unsigned char
{
  COUNTRIES_WORLD = 1,
  WORLDCOASTS = 2
};

class FinalProcessorIntermediateMwmInteface
{
public:
  FinalProcessorIntermediateMwmInteface(FinalProcessorPriority priority) : m_priority(priority) {}
  virtual ~FinalProcessorIntermediateMwmInteface() = default;

  virtual bool Process() = 0;

  bool operator<(FinalProcessorIntermediateMwmInteface const & other) const;
  bool operator==(FinalProcessorIntermediateMwmInteface const & other) const;
  bool operator!=(FinalProcessorIntermediateMwmInteface const & other) const;

protected:
  FinalProcessorPriority m_priority;
};

class CountryFinalProcessor : public FinalProcessorIntermediateMwmInteface
{
public:
  explicit CountryFinalProcessor(std::string const & borderPath,
                                 std::string const & temproryMwmPath,
                                 size_t threadsCount);

  void NeedBookig(std::string const & filename);
  void UseCityBoundaries(std::string const & filename);
  void AddCoastlines(std::string const & coastlineGeomFilename,
                     std::string const & worldCoastsFilename);

  // FinalProcessorIntermediateMwmInteface overrides:
  bool Process() override;

private:
  bool ProcessBooking();
  bool ProcessCityBoundaries();
  bool ProcessCoasline();

  std::string m_borderPath;
  std::string m_temproryMwmPath;
  std::string m_cityBoundariesTmpFilename;
  std::string m_hotelsPath;
  std::string m_coastlineGeomFilename;
  std::string m_worldCoastsFilename;
  size_t m_threadsCount;
};

class WorldFinalProcessor : public FinalProcessorIntermediateMwmInteface
{
public:
  using WorldGenerator = WorldMapGenerator<feature::FeaturesCollector>;

  explicit WorldFinalProcessor(std::string const & worldTmpFilename,
                               std::string const & coastlineGeomFilename,
                               std::string const & popularPlacesFilename);

  void UseCityBoundaries(std::string const & filename);

  // FinalProcessorIntermediateMwmInteface overrides:
  bool Process() override;

private:

  bool ProcessCityBoundaries();

  std::string m_worldTmpFilename;
  std::string m_coastlineGeomFilename;
  std::string m_popularPlacesFilename;
  std::string m_cityBoundariesTmpFilename;
};

class CoastlineFinalProcessor : public FinalProcessorIntermediateMwmInteface
{
public:
  explicit CoastlineFinalProcessor(std::string const & filename);

  void SetCoastlineGeomFilename(std::string const & filename);
  void SetCoastlineRawGeomFilename(std::string const & filename);

  // FinalProcessorIntermediateMwmInteface overrides:
  bool Process() override;

private:
  std::string m_filename;
  std::string m_coastlineGeomFilename;
  std::string m_coastlineRawGeomFilename;
  CoastlineFeaturesGenerator m_generator;
};
}  // namespace generator
