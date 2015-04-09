//
//  osm_entity.h
//  feature_changeset
//
//  Created by Sergey Yershov on 09.04.15.
//  Copyright (c) 2015 Sergey Yershov. All rights reserved.
//

#ifndef __feature_changeset__osm_entity__
#define __feature_changeset__osm_entity__

#include "../std/string.hpp"
#include "../std/vector.hpp"
#include "../std/map.hpp"

namespace edit
{
  struct OsmMember
  {
    string type;
    uint64_t ref;
    string role;

    OsmMember(string const & _type, uint64_t _ref, string _role) : type(_type), ref(_ref), role(_role) {}
  };


  class OsmEntity
  {
    public:
    int64_t id;
    string type;
    uint32_t version;
    uint32_t changeset;
    string user;
    string uid;
    string timestamp;
    string visible;
    double lon;
    double lat;

    vector<uint64_t> nds;
    vector<OsmMember> members;
    map<string, string> tags;
  };

  class DummyOsmParser
  {
    vector<OsmEntity> m_entityList;

    size_t m_depth;

    string m_tmpK;
    string m_tmpV;

    string m_tmpType;
    string m_tmpRole;
    uint64_t m_tmpRef;

  protected:
    OsmEntity * m_current;
    bool m_subelement = false;
    string m_tagName;

  public:
    DummyOsmParser() : m_depth(0), m_current(0) {}

    void AddAttr(string const & key, string const & value);
    bool Push(string const & tagName);
    void Pop(string const &);
    void CharData(string const &) {}

    vector<OsmEntity> const & Results() const { return m_entityList; }
  };


} // namespace edit

#endif /* defined(__feature_changeset__osm_entity__) */
