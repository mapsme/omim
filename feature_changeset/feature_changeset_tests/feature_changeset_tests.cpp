#include "../../testing/testing.hpp"
#include "../featurechangeset.hpp"

#include "../../std/chrono.hpp"
#include "../../std/thread.hpp"



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
    std::remove("changes.db");
    edit::FeatureChangeset changes;
    edit::MWMLink link(1,-2,3);
    edit::TChanges fc; // feature changes
    fc.emplace(edit::NAME, edit::DataValue("","Cool name"));
    fc.emplace(edit::STREET, edit::DataValue("","Lenina"));
    fc.emplace(edit::HOUSENUMBER, edit::DataValue("","8"));

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
    std::remove("changes.db");
    {
      edit::FeatureChangeset changes;
      edit::MWMLink link(1,-2,3);
      edit::TChanges fc; // feature changes
      fc.emplace(edit::NAME, edit::DataValue("","Cool name"));
      fc.emplace(edit::STREET, edit::DataValue("","Lenina"));
      fc.emplace(edit::HOUSENUMBER, edit::DataValue("","8"));
      changes.CreateChange(link, fc);
    }
    edit::FeatureChangeset changes;
    edit::MWMLink link(1,-2,3);
    edit::TChanges fc; // feature changes
    fc.emplace(edit::NAME, edit::DataValue("Cool name","Клевое имя"));
    fc.emplace(edit::STREET, edit::DataValue("Lenina","Ленина"));
    fc.emplace(edit::HOUSENUMBER, edit::DataValue("8","18"));
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
    std::remove("changes.db");
    {
      edit::FeatureChangeset changes;
      edit::MWMLink link(1,-2,3);
      edit::TChanges fc; // feature changes
      fc.emplace(edit::NAME, edit::DataValue("","Cool name"));
      fc.emplace(edit::STREET, edit::DataValue("","Lenina"));
      fc.emplace(edit::HOUSENUMBER, edit::DataValue("","8"));
      changes.CreateChange(link, fc);

      fc.clear();
      fc.emplace(edit::NAME, edit::DataValue("Cool name","Клевое имя"));
      fc.emplace(edit::STREET, edit::DataValue("Lenina","Ленина"));
      fc.emplace(edit::HOUSENUMBER, edit::DataValue("8","18"));
      changes.ModifyChange(link, fc);
    }
    edit::FeatureChangeset changes;
    edit::MWMLink link(1,-2,3);
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
    std::remove("changes.db");
    {
      edit::FeatureChangeset changes;
      edit::MWMLink link(1,-2,3);
      edit::TChanges fc; // feature changes
      fc.emplace(edit::NAME, edit::DataValue("","Cool name"));
      fc.emplace(edit::STREET, edit::DataValue("","Lenina"));
      fc.emplace(edit::HOUSENUMBER, edit::DataValue("","8"));
      changes.CreateChange(link, fc);

      fc.clear();
      fc.emplace(edit::NAME, edit::DataValue("Cool name","Клевое имя"));
      fc.emplace(edit::STREET, edit::DataValue("Lenina","Ленина"));
      fc.emplace(edit::HOUSENUMBER, edit::DataValue("8","18"));
      std::this_thread::sleep_for(std::chrono::seconds(1));
      changes.ModifyChange(link, fc);
      std::this_thread::sleep_for(std::chrono::seconds(1));
      changes.DeleteChange(link);
    }
    edit::FeatureChangeset changes;
    edit::MWMLink link(1,-2,3);
    edit::FeatureDiff diff;

    TEST(changes.Find(link, &diff), ());
    TEST_GREATER_OR_EQUAL((diff.modified-diff.created), 2, ())
    TEST_EQUAL(diff.version, 2, ())
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
