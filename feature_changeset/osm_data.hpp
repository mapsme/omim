#pragma once

#include "../base/string_utils.hpp"
#include "../geometry/point2d.hpp"
#include "../geometry/region2d.hpp"
#include "../std/iostream.hpp"
#include "../std/map.hpp"

namespace osm
{
  typedef int64_t OsmId;
  typedef map<string, string> OsmTags;

  enum class OsmType { NODE, WAY, RELATION, AREA };

  class OsmElement
  {
    friend class OsmParser;
  protected:
    OsmId m_id = 0;
    uint32_t m_version = 1;
    time_t m_timestamp;
    OsmTags m_tags;

    OsmElement(OsmId id) : m_id(id) {}

  public:
    // Type and id
    inline OsmId Id() const { return m_id; }
    inline bool IsNew() const { return m_id <= 0; }
    inline uint32_t Version() const { return m_version; }
    virtual bool IsIncomplete() const = 0;
    virtual OsmType Type() const = 0;
    OsmTags const & Tags() const { return m_tags; }
    // Equality.
    bool operator==(OsmElement const & el) const { return Type() == el.Type() && m_id == el.m_id; }
    // Tags.
    bool HasKey(string const & key) const { return m_tags.count(key); }
    string const GetValue(string const & key) const { return m_tags.at(key); }
    void SetValue(string const & key, string const & value) { m_tags[key] = value; }
    // We use this point to match our features with OSM ones.
    virtual m2::PointD const GetCenter() const = 0;
  };

  class OsmNode : public OsmElement
  {
    m2::PointD m_coords = m2::PointD::Zero();
  public:
    OsmNode(OsmId id) : OsmElement(id) {}
    OsmNode(OsmId id, m2::PointD & coords) : OsmElement(id), m_coords(coords) {}
    OsmNode(m2::PointD & coords) : OsmElement(0), m_coords(coords) {}

    inline OsmType Type() const override { return OsmType::NODE; }
    m2::PointD const GetCenter() const override { return m_coords; }
    bool IsIncomplete() const override { return m_coords.IsAlmostZero(); }
    m2::PointD const & Coords() const { return m_coords; }
    void Coords(m2::PointD coords) { m_coords = coords; }
  };

  // An interface for areas: closed ways and multipolygons
  class OsmArea
  {
  protected:
    m2::RegionD m_area;
  public:
    virtual bool BuildArea() = 0;
    m2::RegionD const & Area() const { return m_area; }
    bool IsArea() const { return m_area.IsValid(); }
  };

  class OsmWay : public OsmElement, public OsmArea
  {
    vector<OsmNode const *> m_nodes;

  public:
    OsmWay(OsmId id) : OsmElement(id) {}
    inline OsmType Type() const override { return OsmType::WAY; }
    inline void Add(OsmNode const * node) { m_nodes.push_back(node); }
    m2::PointD const GetCenter() const override;
    bool IsClosed() const { return m_nodes.size() > 3 && *(m_nodes[0]) == *(m_nodes[m_nodes.size() - 1]); }
    bool IsIncomplete() const override;
    bool BuildArea() override;
    vector<OsmNode const *> const & Nodes() const { return m_nodes; }
  };

  typedef pair<OsmElement const *, string> OsmMember;

  class OsmRelation : public OsmElement, public OsmArea
  {
    vector<OsmMember> m_members;

  public:
    OsmRelation(OsmId id) : OsmElement(id) {}
    inline OsmType Type() const override { return OsmType::RELATION; }
    inline void Add(OsmElement const * element, string const & role = "") { m_members.push_back(OsmMember(element, role)); }
    m2::PointD const GetCenter() const override;
    bool IsMultipolygon() const { return HasKey("type") && GetValue("type") == "multipolygon"; }
    bool IsIncomplete() const override;
    bool BuildArea() override;
    vector<OsmMember> const & Members() const { return m_members; }
  };

  // Just a storage for osm objects
  class OsmData
  {
    map<OsmId, OsmNode> m_nodes;
    map<OsmId, OsmWay> m_ways;
    map<OsmId, OsmRelation> m_relations;

  public:
    void Add(OsmElement * element);
    vector<OsmNode> GetNodes() const;
    vector<OsmWay> GetWays() const;
    vector<OsmRelation> GetRelations() const;
    vector<OsmArea> GetAreas() const;
    vector<OsmElement *> GetElements(OsmType type) const;
    inline bool HasNode(OsmId id) const { return m_nodes.count(id); }
    inline bool HasWay(OsmId id) const { return m_ways.count(id); }
    inline bool HasRelation(OsmId id) const { return m_relations.count(id); }
    inline OsmNode const * GetNode(OsmId id) const { return &m_nodes.at(id); }
    inline OsmWay const * GetWay(OsmId id) const { return &m_ways.at(id); }
    inline OsmRelation const * GetRelation(OsmId id) const { return &m_relations.at(id); }
    bool Empty() const { return m_nodes.empty() && m_ways.empty() && m_relations.empty(); }
    void InnerXML(ostream & oss, OsmId changeset = -1) const;
    void XML(ostream & oss) const;
    bool BuildAreas();
  };

  class OsmChange
  {
    OsmData m_created;
    OsmData m_modified;
    OsmData m_deleted;
    string m_comment;

  public:
    void Create(OsmElement * el) { m_created.Add(el); }
    void Modify(OsmElement * el) { m_modified.Add(el); }
    void Delete(OsmElement * el) { m_deleted.Add(el); }
    // todo: the same for ways and relations
    bool Empty() const { return m_created.Empty() && m_modified.Empty() && m_deleted.Empty(); }
    void Comment(string const & comment ) { m_comment = comment; }
    void XML(ostream & oss, OsmId changeset) const;
    void ChangesetXML(ostream & oss) const;
  };

  class OsmParser
  {
    OsmData & m_data;
    OsmElement *m_element;
    OsmNode m_node;
    OsmWay m_way;
    OsmRelation m_relation;
    string m_key;
    string m_value;
    OsmId m_ref;
    string m_ref_type;
    string m_ref_role;
    string m_tagName;
    m2::PointD m_coords;

  public:
    OsmParser(OsmData & target) : m_data(target), m_node(0), m_way(0), m_relation(0) {}
    bool Push(string const & element);
    OsmElement const * Find();
    void Pop(string const & element);
    void AddAttr(string const & attr, string const & value);
    void CharData(string const &) {}
  };
}
