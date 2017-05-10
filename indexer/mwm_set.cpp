#include "indexer/mwm_set.hpp"
#include "indexer/scales.hpp"

#include "coding/reader.hpp"

#include "base/assert.hpp"
#include "base/exception.hpp"
#include "base/logging.hpp"
#include "base/stl_add.hpp"

#include <algorithm>
#include <exception>
#include <sstream>

#include "defines.hpp"

using platform::CountryFile;
using platform::LocalCountryFile;

MwmInfo::MwmInfo() : m_minScale(0), m_maxScale(0), m_status(STATUS_DEREGISTERED), m_numRefs(0) {}

MwmInfo::MwmTypeT MwmInfo::GetType() const
{
  if (m_minScale > 0)
    return COUNTRY;
  if (m_maxScale == scales::GetUpperWorldScale())
    return WORLD;
  ASSERT_EQUAL(m_maxScale, scales::GetUpperScale(), ());
  return COASTS;
}

std::string DebugPrint(MwmSet::MwmId const & id)
{
  std::ostringstream ss;
  if (id.m_info.get())
    ss << "MwmId [" << id.m_info->GetCountryName() << "]";
  else
    ss << "MwmId [invalid]";
  return ss.str();
}

MwmSet::MwmHandle::MwmHandle() : m_mwmSet(nullptr), m_value(nullptr) {}

MwmSet::MwmHandle::MwmHandle(MwmSet & mwmSet, MwmId const & mwmId,
                             std::unique_ptr<MwmSet::MwmValueBase> && value)
  : m_mwmId(mwmId), m_mwmSet(&mwmSet), m_value(std::move(value))
{
}

MwmSet::MwmHandle::MwmHandle(MwmHandle && handle)
  : m_mwmId(handle.m_mwmId), m_mwmSet(handle.m_mwmSet), m_value(std::move(handle.m_value))
{
  handle.m_mwmSet = nullptr;
  handle.m_mwmId.Reset();
  handle.m_value = nullptr;
}

MwmSet::MwmHandle::~MwmHandle()
{
  if (m_mwmSet && m_value)
    m_mwmSet->UnlockValue(m_mwmId, std::move(m_value));
}

shared_ptr<MwmInfo> const & MwmSet::MwmHandle::GetInfo() const
{
  ASSERT(IsAlive(), ("MwmHandle is not active."));
  return m_mwmId.GetInfo();
}

MwmSet::MwmHandle & MwmSet::MwmHandle::operator=(MwmHandle && handle)
{
  std::swap(m_mwmSet, handle.m_mwmSet);
  std::swap(m_mwmId, handle.m_mwmId);
  std::swap(m_value, handle.m_value);
  return *this;
}

MwmSet::MwmId MwmSet::GetMwmIdByCountryFileImpl(CountryFile const & countryFile) const
{
  std::string const & name = countryFile.GetName();
  ASSERT(!name.empty(), ());
  auto const it = m_info.find(name);
  if (it == m_info.cend() || it->second.empty())
    return MwmId();
  return MwmId(it->second.back());
}

pair<MwmSet::MwmId, MwmSet::RegResult> MwmSet::Register(LocalCountryFile const & localFile)
{
  std::pair<MwmSet::MwmId, MwmSet::RegResult> result;
  auto registerFile = [&](EventList & events)
  {
    CountryFile const & countryFile = localFile.GetCountryFile();
    MwmId const id = GetMwmIdByCountryFileImpl(countryFile);
    if (!id.IsAlive())
    {
      result = RegisterImpl(localFile, events);
      return;
    }

    std::shared_ptr<MwmInfo> info = id.GetInfo();

    // Deregister old mwm for the country.
    if (info->GetVersion() < localFile.GetVersion())
    {
      EventList subEvents;
      DeregisterImpl(id, subEvents);
      result = RegisterImpl(localFile, subEvents);

      // In the case of success all sub-events are
      // replaced with a single UPDATE event. Otherwise,
      // sub-events are reported as is.
      if (result.second == MwmSet::RegResult::Success)
        events.Add(Event(Event::TYPE_UPDATED, localFile, info->GetLocalFile()));
      else
        events.Append(subEvents);
      return;
    }

    std::string const name = countryFile.GetName();
    // Update the status of the mwm with the same version.
    if (info->GetVersion() == localFile.GetVersion())
    {
      LOG(LINFO, ("Updating already registered mwm:", name));
      SetStatus(*info, MwmInfo::STATUS_REGISTERED, events);
      info->m_file = localFile;
      result = std::make_pair(id, RegResult::VersionAlreadyExists);
      return;
    }

    LOG(LWARNING, ("Trying to add too old (", localFile.GetVersion(), ") mwm (", name,
                   "), current version:", info->GetVersion()));
    result = std::make_pair(MwmId(), RegResult::VersionTooOld);
    return;
  };

  WithEventLog(registerFile);
  return result;
}

pair<MwmSet::MwmId, MwmSet::RegResult> MwmSet::RegisterImpl(LocalCountryFile const & localFile,
                                                            EventList & events)
{
  // This function can throw an exception for a bad mwm file.
  std::shared_ptr<MwmInfo> info(CreateInfo(localFile));
  if (!info)
    return std::make_pair(MwmId(), RegResult::UnsupportedFileFormat);

  info->m_file = localFile;
  SetStatus(*info, MwmInfo::STATUS_REGISTERED, events);
  m_info[localFile.GetCountryName()].push_back(info);

  return std::make_pair(MwmId(info), RegResult::Success);
}

bool MwmSet::DeregisterImpl(MwmId const & id, EventList & events)
{
  if (!id.IsAlive())
    return false;

  std::shared_ptr<MwmInfo> const & info = id.GetInfo();
  if (info->m_numRefs == 0)
  {
    SetStatus(*info, MwmInfo::STATUS_DEREGISTERED, events);
    std::vector<std::shared_ptr<MwmInfo>> & infos = m_info[info->GetCountryName()];
    infos.erase(remove(infos.begin(), infos.end(), info), infos.end());
    for (auto it = m_cache.begin(); it != m_cache.end(); ++it)
    {
      if (it->first == id)
      {
        m_cache.erase(it);
        break;
      }
    }
    return true;
  }

  SetStatus(*info, MwmInfo::STATUS_MARKED_TO_DEREGISTER, events);
  return false;
}

bool MwmSet::Deregister(CountryFile const & countryFile)
{
  bool deregistered = false;
  WithEventLog([&](EventList & events)
               {
                 deregistered = DeregisterImpl(countryFile, events);
               });
  return deregistered;
}

bool MwmSet::DeregisterImpl(CountryFile const & countryFile, EventList & events)
{
  MwmId const id = GetMwmIdByCountryFileImpl(countryFile);
  if (!id.IsAlive())
    return false;
  bool const deregistered = DeregisterImpl(id, events);
  ClearCache(id);
  return deregistered;
}

bool MwmSet::IsLoaded(CountryFile const & countryFile) const
{
  std::lock_guard<std::mutex> lock(m_lock);

  MwmId const id = GetMwmIdByCountryFileImpl(countryFile);
  return id.IsAlive() && id.GetInfo()->IsRegistered();
}

void MwmSet::GetMwmsInfo(std::vector<std::shared_ptr<MwmInfo>> & info) const
{
  std::lock_guard<std::mutex> lock(m_lock);
  info.clear();
  info.reserve(m_info.size());
  for (auto const & p : m_info)
  {
    if (!p.second.empty())
      info.push_back(p.second.back());
  }
}

void MwmSet::SetStatus(MwmInfo & info, MwmInfo::Status status, EventList & events)
{
  MwmInfo::Status oldStatus = info.SetStatus(status);
  if (oldStatus == status)
    return;

  switch (status)
  {
  case MwmInfo::STATUS_REGISTERED:
    events.Add(Event(Event::TYPE_REGISTERED, info.GetLocalFile()));
    break;
  case MwmInfo::STATUS_MARKED_TO_DEREGISTER: break;
  case MwmInfo::STATUS_DEREGISTERED:
    events.Add(Event(Event::TYPE_DEREGISTERED, info.GetLocalFile()));
    break;
  }
}

void MwmSet::ProcessEventList(EventList & events)
{
  for (auto const & event : events.Get())
  {
    switch (event.m_type)
    {
    case Event::TYPE_REGISTERED:
      m_observers.ForEach(&Observer::OnMapRegistered, event.m_file);
      break;
    case Event::TYPE_UPDATED:
      m_observers.ForEach(&Observer::OnMapUpdated, event.m_file, event.m_oldFile);
      break;
    case Event::TYPE_DEREGISTERED:
      m_observers.ForEach(&Observer::OnMapDeregistered, event.m_file);
      break;
    }
  }
}

unique_ptr<MwmSet::MwmValueBase> MwmSet::LockValue(MwmId const & id)
{
  std::unique_ptr<MwmSet::MwmValueBase> result;
  WithEventLog([&](EventList & events)
               {
                 result = LockValueImpl(id, events);
               });
  return result;
}

unique_ptr<MwmSet::MwmValueBase> MwmSet::LockValueImpl(MwmId const & id, EventList & events)
{
  if (!id.IsAlive())
    return nullptr;
  std::shared_ptr<MwmInfo> info = id.GetInfo();

  // It's better to return valid "value pointer" even for "out-of-date" files,
  // because they can be locked for a long time by other algos.
  //if (!info->IsUpToDate())
  //  return TMwmValueBasePtr();

  ++info->m_numRefs;

  // Search in cache.
  for (auto it = m_cache.begin(); it != m_cache.end(); ++it)
  {
    if (it->first == id)
    {
      std::unique_ptr<MwmValueBase> result = std::move(it->second);
      m_cache.erase(it);
      return result;
    }
  }

  try
  {
    return CreateValue(*info);
  }
  catch (Reader::TooManyFilesException const & ex)
  {
    LOG(LERROR, ("Too many open files, can't open:", info->GetCountryName()));
    --info->m_numRefs;
    return nullptr;
  }
  catch (std::exception const & ex)
  {
    LOG(LERROR, ("Can't create MWMValue for", info->GetCountryName(), "Reason", ex.what()));

    --info->m_numRefs;
    DeregisterImpl(id, events);
    return nullptr;
  }
}

void MwmSet::UnlockValue(MwmId const & id, std::unique_ptr<MwmValueBase> p)
{
  WithEventLog([&](EventList & events)
               {
                 UnlockValueImpl(id, std::move(p), events);
               });
}

void MwmSet::UnlockValueImpl(MwmId const & id, std::unique_ptr<MwmValueBase> p, EventList & events)
{
  ASSERT(id.IsAlive(), (id));
  ASSERT(p.get() != nullptr, ());
  if (!id.IsAlive() || !p)
    return;

  std::shared_ptr<MwmInfo> const & info = id.GetInfo();
  ASSERT_GREATER(info->m_numRefs, 0, ());
  --info->m_numRefs;
  if (info->m_numRefs == 0 && info->GetStatus() == MwmInfo::STATUS_MARKED_TO_DEREGISTER)
    VERIFY(DeregisterImpl(id, events), ());

  if (info->IsUpToDate())
  {
    /// @todo Probably, it's better to store only "unique by id" free caches here.
    /// But it's no obvious if we have many threads working with the single mwm.

    m_cache.push_back(std::make_pair(id, std::move(p)));
    if (m_cache.size() > m_cacheSize)
    {
      ASSERT_EQUAL(m_cache.size(), m_cacheSize + 1, ());
      m_cache.pop_front();
    }
  }
}

void MwmSet::Clear()
{
  std::lock_guard<std::mutex> lock(m_lock);
  ClearCacheImpl(m_cache.begin(), m_cache.end());
  m_info.clear();
}

void MwmSet::ClearCache()
{
  std::lock_guard<std::mutex> lock(m_lock);
  ClearCacheImpl(m_cache.begin(), m_cache.end());
}

MwmSet::MwmId MwmSet::GetMwmIdByCountryFile(CountryFile const & countryFile) const
{
  std::lock_guard<std::mutex> lock(m_lock);
  return GetMwmIdByCountryFileImpl(countryFile);
}

MwmSet::MwmHandle MwmSet::GetMwmHandleByCountryFile(CountryFile const & countryFile)
{
  MwmSet::MwmHandle handle;
  WithEventLog([&](EventList & events)
               {
                 handle = GetMwmHandleByIdImpl(GetMwmIdByCountryFileImpl(countryFile), events);
               });
  return handle;
}

MwmSet::MwmHandle MwmSet::GetMwmHandleById(MwmId const & id)
{
  MwmSet::MwmHandle handle;
  WithEventLog([&](EventList & events)
               {
                 handle = GetMwmHandleByIdImpl(id, events);
               });
  return handle;
}

MwmSet::MwmHandle MwmSet::GetMwmHandleByIdImpl(MwmId const & id, EventList & events)
{
  std::unique_ptr<MwmValueBase> value;
  if (id.IsAlive())
    value = LockValueImpl(id, events);
  return MwmHandle(*this, id, std::move(value));
}

void MwmSet::ClearCacheImpl(CacheType::iterator beg, CacheType::iterator end)
{
  m_cache.erase(beg, end);
}

void MwmSet::ClearCache(MwmId const & id)
{
  auto sameId = [&id](std::pair<MwmSet::MwmId, std::unique_ptr<MwmSet::MwmValueBase>> const & p)
  {
    return (p.first == id);
  };
  ClearCacheImpl(RemoveIfKeepValid(m_cache.begin(), m_cache.end(), sameId), m_cache.end());
}

std::string DebugPrint(MwmSet::RegResult result)
{
  switch (result)
  {
    case MwmSet::RegResult::Success:
      return "Success";
    case MwmSet::RegResult::VersionAlreadyExists:
      return "VersionAlreadyExists";
    case MwmSet::RegResult::VersionTooOld:
      return "VersionTooOld";
    case MwmSet::RegResult::BadFile:
      return "BadFile";
    case MwmSet::RegResult::UnsupportedFileFormat:
      return "UnsupportedFileFormat";
  }
}

std::string DebugPrint(MwmSet::Event::Type type)
{
  switch (type)
  {
    case MwmSet::Event::TYPE_REGISTERED: return "Registered";
    case MwmSet::Event::TYPE_UPDATED: return "Updated";
    case MwmSet::Event::TYPE_DEREGISTERED: return "Deregistered";
  }
  return "Undefined";
}

std::string DebugPrint(MwmSet::Event const & event)
{
  std::ostringstream os;
  os << "MwmSet::Event [" << DebugPrint(event.m_type) << ", " << DebugPrint(event.m_file);
  if (event.m_type == MwmSet::Event::TYPE_UPDATED)
    os << ", " << DebugPrint(event.m_oldFile);
  os << "]";
  return os.str();
}
