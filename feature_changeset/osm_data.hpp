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
    friend class OsmData;
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
    // Equality.
    bool operator ==(OsmElement const & el) const { return Type() == el.Type() && m_id == el.m_id; }
    // Tags.
    bool HasKey(string const & key) const { return m_tags.count(key); }
    string GetValue(string const & key) const { return m_tags.at(key); }
    void SetValue(string const & key, string const & value) { m_tags[key] = value; }
    // We use this point to match our features with OSM ones.
    virtual m2::PointD GetCenter() const = 0;
  };
  typedef std::reference_wrapper<OsmElement> OsmElementRef;

  class OsmNode : public OsmElement
  {
    m2::PointD m_coords = m2::PointD::Zero();
  public:
    OsmNode(OsmId id) : OsmElement(id) {}
    OsmNode(OsmId id, m2::PointD & coords) : OsmElement(id), m_coords(coords) {}
    OsmNode(m2::PointD & coords) : OsmElement(0), m_coords(coords) {}

    inline OsmType Type() const override { return OsmType::NODE; }
    m2::PointD GetCenter() const override { return m_coords; }
    bool IsIncomplete() const override { return m_coords.IsAlmostZero(); }
    m2::PointD Coords() const { return m_coords; }
  };
  typedef std::reference_wrapper<OsmNode> OsmNodeRef;

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
  typedef std::reference_wrapper<OsmArea> OsmAreaRef;

  class OsmWay : public OsmElement, public OsmArea
  {
    vector<OsmNodeRef> m_nodes;

  public:
    OsmWay(OsmId id) : OsmElement(id) {}
    inline OsmType Type() const override { return OsmType::WAY; }
    inline void Add(OsmNode & node) { m_nodes.push_back(std::ref(node)); }
    m2::PointD GetCenter() const override;
    bool IsClosed() const { return m_nodes.size() > 3 && m_nodes[0].get() == m_nodes[m_nodes.size() - 1].get(); }
    bool IsIncomplete() const override;
    bool BuildArea() override;
    vector<OsmNodeRef> const & Nodes() const { return m_nodes; }
  };
  typedef std::reference_wrapper<OsmWay> OsmWayRef;

  typedef pair<OsmElementRef, string> OsmMember;

  class OsmRelation : public OsmElement, public OsmArea
  {
    vector<OsmMember> m_members;

  public:
    OsmRelation(OsmId id) : OsmElement(id) {}
    inline OsmType Type() const override { return OsmType::RELATION; }
    inline void Add(OsmElement & element, string const & role = "") { m_members.push_back(OsmMember(std::ref(element), role)); }
    m2::PointD GetCenter() const override;
    bool IsMultipolygon() const { return HasKey("type") && GetValue("type") == "multipolygon"; }
    bool IsIncomplete() const override;
    bool BuildArea() override;
    vector<OsmMember> const & Members() const { return m_members; }
  };
  typedef std::reference_wrapper<OsmRelation> OsmRelationRef;

  // Just a storage for osm objects
  class OsmData
  {
    map<OsmId, OsmNodeRef> m_nodes;
    map<OsmId, OsmWayRef> m_ways;
    map<OsmId, OsmRelationRef> m_relations;

  public:
    void Add(OsmElement & element);
    vector<OsmNodeRef> & GetNodes() const;
    vector<OsmWayRef> & GetWays() const;
    vector<OsmRelationRef> & GetRelations() const;
    vector<OsmAreaRef> & GetAreas() const;
    vector<OsmElementRef> & GetElements(OsmType type) const;
    bool HasNode(OsmId id) const { return m_nodes.count(id); }
    bool HasWay(OsmId id) const { return m_ways.count(id); }
    bool HasRelation(OsmId id) const { return m_relations.count(id); }
    OsmNode & GetNode(OsmId id) const { return m_nodes.at(id); }
    OsmWay & GetWay(OsmId id) const { return m_ways.at(id); }
    OsmRelation & GetRelation(OsmId id) const { return m_relations.at(id); }
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
    void Create(OsmElement & el) { m_created.Add(el); }
    void Modify(OsmElement & el) { m_modified.Add(el); }
    void Delete(OsmElement & el) { m_deleted.Add(el); }
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
    string m_key;
    string m_value;
    OsmId m_ref;
    string m_ref_type;
    string m_ref_role;
    string m_tagName;

  public:
    OsmParser(OsmData & target) : m_data(target) {}
    bool Push(string const & element);
    //OsmElement & FindRef();
    //void Pop(string const & element);
    //void AddAttr(string const & attr, string const & value);
    void CharData(string const &) {}
  };
}
