#pragma once

#include "ugc/types.hpp"

#include "base/thread_checker.hpp"

#include <memory>
#include <string>

class DataSource;
class FeatureType;
struct FeatureID;

namespace ugc
{
class Storage
{
public:
  explicit Storage(DataSource const & dataSource) : m_dataSource(dataSource) {}

  UGCUpdate GetUGCUpdate(FeatureID const & id) const;

  enum class SettingResult
  {
    Success,
    InvalidUGC,
    WritingError
  };

  SettingResult SetUGCUpdate(FeatureID const & id, UGCUpdate const & ugc);
  bool SaveIndex(std::string const & pathToTargetFile = "") const;
  std::string GetUGCToSend() const;
  void MarkAllAsSynchronized();
  void Defragmentation();
  void Load();
  size_t GetNumberOfUnsynchronized() const;

  /// Testing
  UpdateIndexes & GetIndexesForTesting() { return m_indexes; }
  size_t GetNumberOfDeletedForTesting() const { return m_numberOfDeleted; }
  SettingResult SetUGCUpdateForTesting(FeatureID const & id, v0::UGCUpdate const & ugc);
  void LoadForTesting(std::string const & testIndexFilePath);

private:
  void DefragmentationImpl(bool force);
  uint64_t UGCSizeAtIndex(size_t const indexPosition) const;
  std::unique_ptr<FeatureType> GetFeature(FeatureID const & id) const;
  void Migrate(std::string const & indexFilePath);

  DataSource const & m_dataSource;
  UpdateIndexes m_indexes;
  size_t m_numberOfDeleted = 0;
};

inline std::string DebugPrint(Storage::SettingResult const & result)
{
  switch (result)
  {
  case Storage::SettingResult::Success: return "Success";
  case Storage::SettingResult::InvalidUGC: return "Invalid UGC";
  case Storage::SettingResult::WritingError: return "Writing Error";
  }
  CHECK_SWITCH();
}
}  // namespace ugc

namespace lightweight
{
size_t GetNumberOfUnsentUGC();
}  //namespace lightweight
