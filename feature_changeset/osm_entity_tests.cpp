//
//  osm_entity_tests.cpp
//  feature_changeset
//
//  Created by Sergey Yershov on 09.04.15.
//  Copyright (c) 2015 Sergey Yershov. All rights reserved.
//

#include "../testing/testing.hpp"
#include "osm_entity.hpp"

#include "../coding/parse_xml.hpp"
#include "../generator/source_reader.hpp"


UNIT_TEST(OsmEntity_Base)
{
  edit::DummyOsmParser parser;
  SourceReader source("test.osm.xml");

  ParseXMLSequence(source, parser);

  TEST(!parser.Results().empty(), ());

  for (auto const e : parser.Results())
  {
    cout << e.type << " nd: " << e.nds.size() << " members: " << e.members.size() << " tags: " << e.tags.size() << endl;
  }

  cout << parser.Results().size() << endl;
}
