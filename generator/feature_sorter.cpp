#include "generator/feature_sorter.hpp"

#include "generator/borders.hpp"
#include "generator/feature_builder.hpp"
#include "generator/feature_generator.hpp"
#include "generator/gen_mwm_info.hpp"
#include "generator/geometry_holder.hpp"
#include "generator/region_meta.hpp"
#include "generator/tesselator.hpp"

#include "routing/routing_helpers.hpp"
#include "routing/speed_camera_prohibition.hpp"

#include "indexer/classificator.hpp"
#include "indexer/dat_section_header.hpp"
#include "indexer/feature_algo.hpp"
#include "indexer/feature_impl.hpp"
#include "indexer/feature_processor.hpp"
#include "indexer/feature_visibility.hpp"
#include "indexer/meta_idx.hpp"
#include "indexer/scales.hpp"
#include "indexer/scales_patch.hpp"

#include "platform/country_file.hpp"
#include "platform/mwm_version.hpp"
#include "platform/platform.hpp"

#include "coding/files_container.hpp"
#include "coding/internal/file_data.hpp"
#include "coding/point_coding.hpp"
#include "coding/succinct_mapper.hpp"

#include "geometry/polygon.hpp"

#include "base/assert.hpp"
#include "base/file_name_utils.hpp"
#include "base/logging.hpp"
#include "base/scope_guard.hpp"
#include "base/string_utils.hpp"

#include "defines.hpp"

#include <limits>
#include <list>
#include <memory>
#include <vector>

using namespace std;

namespace feature
{
class FeaturesCollector2 : public FeaturesCollector
{
public:
  static uint32_t constexpr kInvalidFeatureId = std::numeric_limits<uint32_t>::max();

  FeaturesCollector2(string const & filename, DataHeader const & header,
                     RegionData const & regionData, uint32_t versionDate)
    : FeaturesCollector(filename + FEATURES_FILE_TAG)
    , m_filename(filename)
    , m_header(header)
    , m_regionData(regionData)
    , m_versionDate(versionDate)
  {
    for (size_t i = 0; i < m_header.GetScalesCount(); ++i)
    {
      string const postfix = strings::to_string(i);
      m_geoFile.push_back(make_unique<TmpFile>(filename + GEOMETRY_FILE_TAG + postfix));
      m_trgFile.push_back(make_unique<TmpFile>(filename + TRIANGLE_FILE_TAG + postfix));
    }

    m_metadataFile = make_unique<TmpFile>(filename + METADATA_FILE_TAG);
    m_addrFile = make_unique<FileWriter>(filename + TEMP_ADDR_FILENAME);
  }

  void Finish()
  {
    // write version information
    {
      FilesContainerW writer(m_filename);
      auto w = writer.GetWriter(VERSION_FILE_TAG);
      version::WriteVersion(*w, m_versionDate);
    }

    // write own mwm header
    m_header.SetBounds(m_bounds);
    {
      FilesContainerW writer(m_filename, FileWriter::OP_WRITE_EXISTING);
      auto w = writer.GetWriter(HEADER_FILE_TAG);
      m_header.Save(*w);
    }

    // write region info
    {
      FilesContainerW writer(m_filename, FileWriter::OP_WRITE_EXISTING);
      auto w = writer.GetWriter(REGION_INFO_FILE_TAG);
      m_regionData.Serialize(*w);
    }

    // assume like we close files
    Flush();

    {
      FilesContainerW writer(m_filename, FileWriter::OP_WRITE_EXISTING);
      auto w = writer.GetWriter(FEATURES_FILE_TAG);

      size_t const startOffset = w->Pos();
      CHECK(coding::IsAlign8(startOffset), ());

      feature::DatSectionHeader header;
      header.Serialize(*w);

      uint64_t bytesWritten = static_cast<uint64_t>(w->Pos());
      coding::WritePadding(*w, bytesWritten);

      header.m_featuresOffset = base::asserted_cast<uint32_t>(w->Pos() - startOffset);
      ReaderSource<ModelReaderPtr> src(make_unique<FileReader>(m_dataFile.GetName()));
      rw::ReadAndWrite(src, *w);
      header.m_featuresSize =
          base::asserted_cast<uint32_t>(w->Pos() - header.m_featuresOffset - startOffset);

      auto const endOffset = w->Pos();
      w->Seek(startOffset);
      header.Serialize(*w);
      w->Seek(endOffset);
    }

    // File Writer finalization function with adding section to the main mwm file.
    auto const finalizeFn = [this](unique_ptr<TmpFile> w, string const & tag,
                                   string const & postfix = string()) {
      w->Flush();
      FilesContainerW writer(m_filename, FileWriter::OP_WRITE_EXISTING);
      writer.Write(w->GetName(), tag + postfix);
    };

    for (size_t i = 0; i < m_header.GetScalesCount(); ++i)
    {
      string const postfix = strings::to_string(i);
      finalizeFn(move(m_geoFile[i]), GEOMETRY_FILE_TAG, postfix);
      finalizeFn(move(m_trgFile[i]), TRIANGLE_FILE_TAG, postfix);
    }

    finalizeFn(move(m_metadataFile), METADATA_FILE_TAG);

    {
      FilesContainerW writer(m_filename, FileWriter::OP_WRITE_EXISTING);
      auto w = writer.GetWriter(METADATA_INDEX_FILE_TAG);

      MetadataIndexBuilder metaIdxBuilder;
      for (auto const & v : m_metadataOffset)
        metaIdxBuilder.Put(v.first, v.second);

      metaIdxBuilder.Freeze(*w);
    }

    if (m_header.GetType() == DataHeader::MapType::Country ||
        m_header.GetType() == DataHeader::MapType::World)
    {
      FileWriter osm2ftWriter(m_filename + OSM2FEATURE_FILE_EXTENSION);
      m_osm2ft.Write(osm2ftWriter);
    }
  }

  void SetBounds(m2::RectD bounds) { m_bounds = bounds; }

  uint32_t operator()(FeatureBuilder & fb)
  {
    GeometryHolder holder([this](int i) -> FileWriter & { return *m_geoFile[i]; },
                          [this](int i) -> FileWriter & { return *m_trgFile[i]; }, fb, m_header);

    bool const isLine = fb.IsLine();
    bool const isArea = fb.IsArea();

    int const scalesStart = static_cast<int>(m_header.GetScalesCount()) - 1;
    for (int i = scalesStart; i >= 0; --i)
    {
      int const level = m_header.GetScale(i);
      if (fb.IsDrawableInRange(i > 0 ? m_header.GetScale(i - 1) + 1 : 0, PatchScaleBound(level)))
      {
        bool const isCoast = fb.IsCoastCell();
        m2::RectD const rect = fb.GetLimitRect();

        // Simplify and serialize geometry.
        Points points;

        // Do not change linear geometry for the upper scale.
        if (isLine && i == scalesStart && IsCountry() && routing::IsRoad(fb.GetTypes()))
          points = holder.GetSourcePoints();
        else
          SimplifyPoints(level, isCoast, rect, holder.GetSourcePoints(), points);

        if (isLine)
          holder.AddPoints(points, i);

        if (isArea && holder.NeedProcessTriangles())
        {
          // simplify and serialize triangles
          bool const good = isCoast || IsGoodArea(points, level);

          // At this point we don't need last point equal to first.
          CHECK_GREATER(points.size(), 0, ());
          points.pop_back();

          Polygons const & polys = fb.GetGeometry();
          if (polys.size() == 1 && good && holder.TryToMakeStrip(points))
            continue;

          Polygons simplified;
          if (good)
          {
            simplified.push_back({});
            simplified.back().swap(points);
          }

          auto iH = polys.begin();
          for (++iH; iH != polys.end(); ++iH)
          {
            simplified.push_back({});

            SimplifyPoints(level, isCoast, rect, *iH, simplified.back());

            // Increment level check for coastline polygons for the first scale level.
            // This is used for better coastlines quality.
            if (IsGoodArea(simplified.back(), (isCoast && i == 0) ? level + 1 : level))
            {
              // At this point we don't need last point equal to first.
              CHECK_GREATER(simplified.back().size(), 0, ());
              simplified.back().pop_back();
            }
            else
            {
              // Remove small polygon.
              simplified.pop_back();
            }
          }

          if (!simplified.empty())
            holder.AddTriangles(simplified, i);
        }
      }
    }

    uint32_t featureId = kInvalidFeatureId;
    auto & buffer = holder.GetBuffer();
    if (fb.PreSerializeAndRemoveUselessNamesForMwm(buffer))
    {
      fb.SerializeForMwm(buffer, m_header.GetDefGeometryCodingParams());

      featureId = WriteFeatureBase(buffer.m_buffer, fb);

      fb.GetAddressData().Serialize(*m_addrFile);

      if (!fb.GetMetadata().Empty())
      {
        uint64_t const offset = m_metadataFile->Pos();
        ASSERT_LESS_OR_EQUAL(offset, numeric_limits<uint32_t>::max(), ());

        m_metadataOffset.emplace_back(featureId, static_cast<uint32_t>(offset));
        fb.GetMetadata().Serialize(*m_metadataFile);
      }

      if (fb.HasOsmIds())
        m_osm2ft.AddIds(generator::MakeCompositeId(fb), featureId);
    };
    return featureId;
  }

private:
  using Points = vector<m2::PointD>;
  using Polygons = list<Points>;

  class TmpFile : public FileWriter
  {
  public:
    explicit TmpFile(string const & filePath) : FileWriter(filePath) {}
    ~TmpFile() { DeleteFileX(GetName()); }
  };

  using TmpFiles = vector<unique_ptr<TmpFile>>;

  static bool IsGoodArea(Points const & poly, int level)
  {
    // Area has the same first and last points. That's why minimal number of points for
    // area is 4.
    if (poly.size() < 4)
      return false;

    m2::RectD r;
    CalcRect(poly, r);

    return scales::IsGoodForLevel(level, r);
  }

  bool IsCountry() const { return m_header.GetType() == feature::DataHeader::MapType::Country; }

  void SimplifyPoints(int level, bool isCoast, m2::RectD const & rect, Points const & in,
                      Points & out)
  {
    if (isCoast)
    {
      DistanceToSegmentWithRectBounds fn(rect);
      feature::SimplifyPoints(fn, level, in, out);
    }
    else
    {
      feature::SimplifyPoints(m2::SquaredDistanceFromSegmentToPoint<m2::PointD>(), level, in, out);
    }
  }

  string m_filename;

  // File used for postcodes and search sections build.
  unique_ptr<FileWriter> m_addrFile;

  // Temporary files for sections.
  unique_ptr<TmpFile> m_metadataFile;
  TmpFiles m_geoFile, m_trgFile;

  // Mapping from feature id to offset in file section with the correspondent metadata.
  vector<pair<uint32_t, uint32_t>> m_metadataOffset;

  DataHeader m_header;
  RegionData m_regionData;
  uint32_t m_versionDate;

  generator::OsmID2FeatureID m_osm2ft;

  DISALLOW_COPY_AND_MOVE(FeaturesCollector2);
};

bool GenerateFinalFeatures(feature::GenerateInfo const & info, string const & name,
                           feature::DataHeader::MapType mapType)
{
  string const srcFilePath = info.GetTmpFileName(name);
  string const dataFilePath = info.GetTargetFileName(name);

  // Store cellIds for middle points.
  CalculateMidPoints midPoints;
  ForEachFeatureRawFormat(srcFilePath, [&midPoints](FeatureBuilder const & fb, uint64_t pos) {
    midPoints(fb, pos);
  });

  // Sort features by their middle point.
  midPoints.Sort();

  // Store sorted features.
  {
    FileReader reader(srcFilePath);
    // Fill mwm header.
    DataHeader header;

    bool const isWorldOrWorldCoasts = (mapType != DataHeader::MapType::Country);
    uint8_t coordBits = kFeatureSorterPointCoordBits;
    if (isWorldOrWorldCoasts)
      coordBits -= ((scales::GetUpperScale() - scales::GetUpperWorldScale()) / 2);

    header.SetType(static_cast<DataHeader::MapType>(mapType));
    header.SetGeometryCodingParams(serial::GeometryCodingParams(coordBits, midPoints.GetCenter()));
    if (isWorldOrWorldCoasts)
      header.SetScales(g_arrWorldScales);
    else
      header.SetScales(g_arrCountryScales);

    RegionData regionData;
    if (!ReadRegionData(name, regionData))
      LOG(LWARNING, ("No extra data for country:", name));

    // Transform features from raw format to optimized format.
    try
    {
      // FeaturesCollector2 will create temporary file `dataFilePath + FEATURES_FILE_TAG`.
      // We cannot remove it in ~FeaturesCollector2(), we need to remove it in SCOPE_GUARD.
      SCOPE_GUARD(_, [&]() { Platform::RemoveFileIfExists(dataFilePath + FEATURES_FILE_TAG); });
      FeaturesCollector2 collector(dataFilePath, header, regionData, info.m_versionDate);

      for (auto const & point : midPoints.GetVector())
      {
        ReaderSource<FileReader> src(reader);
        src.Skip(point.second);

        FeatureBuilder fb;
        ReadFromSourceRawFormat(src, fb);
        collector(fb);
      }

      // Update bounds with the limit rect corresponding to region borders.
      // Bounds before update can be too big because of big invisible features like a
      // relation that contains an entire country's border.
      // Borders file may be unavailable when building test mwms.
      m2::RectD bordersRect;
      if (borders::GetBordersRect(info.m_targetDir, name, bordersRect))
        collector.SetBounds(bordersRect);

      collector.Finish();
    }
    catch (RootException const & ex)
    {
      LOG(LCRITICAL, ("MWM writing error:", ex.Msg()));
      return false;
    }
  }

  return true;
}
}  // namespace feature
