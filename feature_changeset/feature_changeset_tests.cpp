#include <chrono>

#include "../testing/testing.hpp"
#include "featurechangeset.h"

#include "../std/thread.hpp"



UNIT_TEST(Changes_Init)
{
  try
  {
    std::remove("changes.db");
    edit::FeatureChangeset changes;
  }
  catch (edit::Exception & e)
  {
    TEST(false, (e.what()));
  }
}


UNIT_TEST(Changes_Create)
{
  try
  {
    edit::FeatureChangeset changes;
    edit::MWMLink link(1,-2,3);
    edit::TChanges fc; // feature changes
    fc.emplace(edit::NAME, edit::DataValue("","Cool name"));
    fc.emplace(edit::ADDR_STREET, edit::DataValue("","Lenina"));
    fc.emplace(edit::ADDR_HOUSENUMBER, edit::DataValue("","8"));

    changes.CreateChange(link, fc);
  }
  catch (edit::Exception & e)
  {
    TEST(false, (e.what()));
  }
}

UNIT_TEST(Changes_Modify)
{
  try
  {
    edit::FeatureChangeset changes;
    edit::MWMLink link(1,-2,3);
    edit::TChanges fc; // feature changes
    fc.emplace(edit::NAME, edit::DataValue("Cool name","Клевое имя"));
    fc.emplace(edit::ADDR_STREET, edit::DataValue("Lenina","Ленина"));
    fc.emplace(edit::ADDR_HOUSENUMBER, edit::DataValue("8","18"));
    std::this_thread::sleep_for(std::chrono::seconds(1));
    changes.ModifyChange(link, fc);
  }
  catch (edit::Exception & e)
  {
    TEST(false, (e.what()));
  }
}

UNIT_TEST(Changes_Delete)
{
  try
  {
    edit::FeatureChangeset changes;
    edit::MWMLink link(1,-2,3);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    changes.DeleteChange(link);
  }
  catch (edit::Exception & e)
  {
    TEST(false, (e.what()));
  }
}

UNIT_TEST(Changes_Find)
{
  try
  {
    edit::FeatureChangeset changes;
    edit::MWMLink link(1,-2,3);
    edit::FeatureDiff diff;
    TEST(changes.Find(link, &diff), ());
  }
  catch (edit::Exception & e)
  {
    TEST(false, (e.what()));
  }
}

UNIT_TEST(Changes_Cleanup)
{
  std::remove("changes.db");
}
