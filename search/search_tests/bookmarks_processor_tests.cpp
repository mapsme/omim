#include "testing/testing.hpp"

#include "search/bookmarks/data.hpp"
#include "search/bookmarks/processor.hpp"
#include "search/emitter.hpp"

#include "indexer/search_delimiters.hpp"
#include "indexer/search_string_utils.hpp"

#include "base/cancellable.hpp"

#include <string>
#include <vector>

using namespace search::bookmarks;
using namespace search;
using namespace std;

namespace
{
using Ids = vector<Id>;

class BookmarksProcessorTest
{
public:
  BookmarksProcessorTest() : m_processor(m_emitter, m_cancellable) {}

  Processor & GetProcessor() { return m_processor; }

  void Add(Id const & id, GroupId const & group, Doc const & doc)
  {
    m_processor.Add(id, doc);
    AttachToGroup(id, group);
  }

  void Erase(Id const & id) { m_processor.Erase(id); }

  void Update(Id const & id, Doc const & doc)
  {
    m_processor.Update(id, doc);
  }

  void AttachToGroup(Id const & id, GroupId const & group) { m_processor.AttachToGroup(id, group); }
  void DetachFromGroup(Id const & id, GroupId const & group)
  {
    m_processor.DetachFromGroup(id, group);
  }

  Ids Search(string const & query, GroupId const & groupId = kInvalidGroupId)
  {
    m_emitter.Init([](::search::Results const & /* results */) {} /* onResults */);

    vector<strings::UniString> tokens;
    auto const isPrefix =
        TokenizeStringAndCheckIfLastTokenIsPrefix(query, tokens, search::Delimiters());

    Processor::Params params;
    if (isPrefix)
    {
      ASSERT(!tokens.empty(), ());
      params.InitWithPrefix(tokens.begin(), tokens.end() - 1, tokens.back());
    }
    else
    {
      params.InitNoPrefix(tokens.begin(), tokens.end());
    }

    params.m_groupId = groupId;

    m_processor.Search(params);
    Ids ids;
    for (auto const & result : m_emitter.GetResults().GetBookmarksResults())
      ids.emplace_back(result.m_id);
    return ids;
  }

protected:
  Emitter m_emitter;
  base::Cancellable m_cancellable;
  Processor m_processor;
};

kml::BookmarkData MakeBookmarkData(string const & name, string const & customName,
                                   string const & description)
{
  kml::BookmarkData b;
  b.m_name = {{kml::kDefaultLangCode, name}};
  b.m_customName = {{kml::kDefaultLangCode, customName}};
  b.m_description = {{kml::kDefaultLangCode, description}};
  return b;
}

UNIT_CLASS_TEST(BookmarksProcessorTest, Smoke)
{
  GetProcessor().EnableIndexingOfDescriptions(true);

  Add(10, 0, MakeBookmarkData("Double R Diner" /* name */,
                              "2R Diner" /* customName */,
                              "They've got a cherry pie there that'll kill ya!" /* description */));

  Add(18, 0, MakeBookmarkData("Silver Mustang Casino" /* name */,
                              "Ag Mustang" /* customName */,
                              "Joyful place, owners Bradley and Rodney are very friendly!"));
  Add(20, 1, MakeBookmarkData("Great Northern Hotel" /* name */,
                              "N Hotel" /* customName */,
                              "Clean place with a reasonable price" /* description */));

  TEST_EQUAL(Search("R&R food"), Ids{}, ());
  GetProcessor().EnableIndexingOfBookmarkGroup(GroupId{0}, true /* enable */);
  TEST_EQUAL(Search("R&R food"), Ids({10}), ());
  GetProcessor().EnableIndexingOfBookmarkGroup(GroupId{0}, false /* enable */);
  TEST_EQUAL(Search("R&R food"), Ids{}, ());
  GetProcessor().EnableIndexingOfBookmarkGroup(GroupId{0}, true /* enable */);
  TEST_EQUAL(Search("R&R food"), Ids({10}), ());

  GetProcessor().EnableIndexingOfBookmarkGroup(GroupId{1}, true /* enable */);

  TEST_EQUAL(Search("cherry pie"), Ids({10}), ());
  TEST_EQUAL(Search("great silver hotel"), Ids({20, 18}), ());
  TEST_EQUAL(Search("double r cafe"), Ids({10}), ());
  TEST_EQUAL(Search("dine"), Ids({10}), ());
  TEST_EQUAL(Search("2R"), Ids({10}), ());
  TEST_EQUAL(Search("Ag"), Ids({18}), ());

  TEST_EQUAL(Search("place"), Ids({20, 18}), ());
  TEST_EQUAL(Search("place", GroupId{0}), Ids({18}), ());
  DetachFromGroup(20, 1);
  AttachToGroup(20, 0);
  TEST_EQUAL(Search("place", GroupId{0}), Ids({20, 18}), ());

  Update(20, MakeBookmarkData("Great Northern Hotel" /* name */,
                              "N Hotel" /* customName */,
                              "Clean establishment with a reasonable price" /* description */));
  TEST_EQUAL(Search("place", GroupId{0}), Ids({18}), ());
}

UNIT_CLASS_TEST(BookmarksProcessorTest, IndexDescriptions)
{
  GetProcessor().EnableIndexingOfDescriptions(true);
  GetProcessor().EnableIndexingOfBookmarkGroup(GroupId{0}, true /* enable */);

  Add(10, 0, MakeBookmarkData("Double R Diner" /* name */,
                           "2R Diner" /* customName */,
                           "They've got a cherry pie there that'll kill ya!" /* description */));
  TEST_EQUAL(Search("diner"), Ids({10}), ());
  TEST_EQUAL(Search("cherry pie"), Ids({10}), ());

  DetachFromGroup(10, 0);
  Erase(10);
  TEST_EQUAL(Search("diner"), Ids{}, ());
  TEST_EQUAL(Search("cherry pie"), Ids{}, ());

  GetProcessor().EnableIndexingOfDescriptions(false);
  Add(10, 0, MakeBookmarkData("Double R Diner" /* name */,
                           "2R Diner" /* customName */,
                           "They've got a cherry pie there that'll kill ya!" /* description */));
  TEST_EQUAL(Search("diner"), Ids({10}), ());
  TEST_EQUAL(Search("cherry pie"), Ids{}, ());

  // Results for already indexed bookmarks don't change.
  GetProcessor().EnableIndexingOfDescriptions(true);
  TEST_EQUAL(Search("diner"), Ids({10}), ());
  TEST_EQUAL(Search("cherry pie"), Ids{}, ());

  DetachFromGroup(10, 0);
  Erase(10);
  TEST_EQUAL(Search("diner"), Ids{}, ());
  TEST_EQUAL(Search("cherry pie"), Ids{}, ());
}
}  // namespace
