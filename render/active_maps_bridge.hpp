#pragma once

#include "storage/storage_defines.hpp"
#include "storage/index.hpp"

#include "geometry/point2d.hpp"

#include "std/string.hpp"
#include "std/function.hpp"

namespace rg
{

class ActiveMapsBridge
{
public:
  virtual string GetCountryName(storage::TIndex const & index) const = 0;
  virtual storage::TStatus GetCountryStatus(storage::TIndex const & index) const = 0;
  virtual storage::LocalAndRemoteSizeT GetDownloadCountrySize(storage::TIndex const & index) const = 0;
  virtual storage::LocalAndRemoteSizeT GetRemoteCountrySize(storage::TIndex const & index) const = 0;
  virtual void DownloadMap(storage::TIndex const & index, int options) = 0;
  virtual bool IsCountryLoaded(m2::PointD const & pt) const = 0;
  virtual storage::TIndex GetCountryIndex(m2::PointD const & pt) const = 0;

  using TCountryStatusListener = function<void (storage::TIndex const &, storage::TStatus const &)>;
  using TCountryDownloadListener = function<void (storage::TIndex const &, storage::LocalAndRemoteSizeT const &)>;
  void SetStatusListener(TCountryStatusListener const & fn);
  void SetDownloadListener(TCountryDownloadListener const & fn);
  void ResetListeners();

protected:
  void CallStatusListener(storage::TIndex const & index, storage::TStatus const & status);
  void CallDownloadListener(storage::TIndex const & index, storage::LocalAndRemoteSizeT const & sizes);

private:
  TCountryStatusListener m_statusListener;
  TCountryDownloadListener m_progressListener;
};

}
