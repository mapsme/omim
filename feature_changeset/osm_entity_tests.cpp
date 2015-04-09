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


class SourceReader
{
  string const &m_filename;
  FILE * m_file;

public:
  SourceReader(string const & filename) : m_filename(filename)
  {
    if (m_filename.empty())
    {
      LOG(LINFO, ("Read OSM data from stdin..."));
      m_file = freopen(NULL, "rb", stdin);
    }
    else
    {
      LOG(LINFO, ("Read OSM data from", filename));
      m_file = fopen(filename.c_str(), "rb");
    }
  }

  ~SourceReader()
  {
    if (!m_filename.empty())
      fclose(m_file);
  }

  inline FILE * Handle() { return m_file; }

  uint64_t Read(char * buffer, uint64_t bufferSize)
  {
    return fread(buffer, sizeof(char), bufferSize, m_file);
  }
};


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
