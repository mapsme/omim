#pragma once

#include <string>
#include <map>
#include <mutex>

namespace settings
{
/// Current location state mode. @See location::EMyPositionMode.
extern char const * kLocationStateMode;
/// Metric or Feet.
extern char const * kMeasurementUnits;

template <class T>
bool FromString(std::string const & str, T & outValue);
template <class T>
std::string ToString(T const & value);

class StringStorage
{
  typedef std::map<std::string, std::string> ContainerT;
  ContainerT m_values;

  mutable std::mutex m_mutex;

  StringStorage();
  void Save() const;

public:
  static StringStorage & Instance();

  void Clear();
  bool GetValue(std::string const & key, std::string & outValue) const;
  void SetValue(std::string const & key, std::string && value);
  void DeleteKeyAndValue(std::string const & key);
};

/// Retrieve setting
/// @return false if setting is absent
template <class ValueT>
bool Get(std::string const & key, ValueT & outValue)
{
  std::string strVal;
  return StringStorage::Instance().GetValue(key, strVal) && FromString(strVal, outValue);
}
/// Automatically saves setting to external file
template <class ValueT>
void Set(std::string const & key, ValueT const & value)
{
  StringStorage::Instance().SetValue(key, ToString(value));
}

inline void Delete(std::string const & key) { StringStorage::Instance().DeleteKeyAndValue(key); }
inline void Clear() { StringStorage::Instance().Clear(); }

/// Use this function for running some stuff once according to date.
/// @param[in]  date  Current date in format yymmdd.
bool IsFirstLaunchForDate(int date);
}
