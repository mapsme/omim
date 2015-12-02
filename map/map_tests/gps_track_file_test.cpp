#include "testing/testing.hpp"

#include "map/gps_track_file.hpp"

#include "platform/platform.hpp"

#include "coding/file_name_utils.hpp"

#include "std/chrono.hpp"

UNIT_TEST(GpsTrackFile_SimpleWriteRead)
{
  time_t const timestamp = system_clock::to_time_t(system_clock::now());
  string const filePath = my::JoinFoldersToPath(GetPlatform().WritableDir(), "gpstrack.bin");
  size_t const fileMaxItemCount = 100000;

  // Write GPS tracks.
  {
    GpsTrackFile file(filePath, fileMaxItemCount);

    file.Clear();

    TEST_EQUAL(0, file.GetCount(), ());

    for (size_t i = 0; i < fileMaxItemCount/2; ++i)
    {
      file.Append(timestamp + i, m2::PointD(i+1000,i+2000), i+3000);
    }

    TEST_EQUAL(fileMaxItemCount, file.GetCount(), ());

    file.Close();
  }

  // Read GPS tracks.
  {
    GpsTrackFile file(filePath, fileMaxItemCount);

    TEST_EQUAL(fileMaxItemCount, file.GetCount(), ());

    size_t i = 0;
    file.ForEach([&i,timestamp](double t, m2::PointD const & pt, double speed)->bool
    {
      TEST_EQUAL(timestamp + i, t, ());
      TEST_EQUAL(pt.x, i + 1000, ());
      TEST_EQUAL(pt.y, i + 2000, ());
      TEST_EQUAL(speed, i + 3000, ());
      ++i;
      return true;
    });

    file.Close();

    TEST_EQUAL(i, fileMaxItemCount/2, ());
  }
}

UNIT_TEST(GpsTrackFile_WriteReadWithPopping)
{
  time_t const timestamp = system_clock::to_time_t(system_clock::now());
  string const filePath = my::JoinFoldersToPath(GetPlatform().WritableDir(), "gpstrack.bin");
  size_t const fileMaxItemCount = 100000;

  // Write GPS tracks.
  // 2 x fileMaxItemCount more items are written in cyclic file, first half will be popped
  {
    GpsTrackFile file(filePath, fileMaxItemCount);

    file.Clear();

    TEST_EQUAL(0, file.GetCount(), ());

    for (size_t i = 0; i < 2 * fileMaxItemCount; ++i)
    {
      file.Append(timestamp + i, m2::PointD(i+1000,i+2000), i+3000);
    }

    TEST_EQUAL(fileMaxItemCount, file.GetCount(), ());

    file.Close();
  }

  // Read GPS tracks
  // Only last fileMaxItemCount must be in cyclic buffer
  {
    GpsTrackFile file(filePath, fileMaxItemCount);

    TEST_EQUAL(fileMaxItemCount, file.GetCount(), ());

    size_t i = 0;
    file.ForEach([&i,timestamp](double t, m2::PointD const & pt, double speed)->bool
    {
      TEST_EQUAL(timestamp + i + fileMaxItemCount, t, ());
      TEST_EQUAL(pt.x, i + 1000 + fileMaxItemCount, ());
      TEST_EQUAL(pt.y, i + 2000 + fileMaxItemCount, ());
      TEST_EQUAL(speed, i + 3000 + fileMaxItemCount, ());
      ++i;
      return true;
    });

    file.Close();

    TEST_EQUAL(i, fileMaxItemCount, ());
  }
}
