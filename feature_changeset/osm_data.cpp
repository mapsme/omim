#include "osm_data.hpp"

namespace osm
{
  m2::PointD const OsmWay::GetCenter() const
  {
    CHECK(!m_nodes.empty() && !IsIncomplete(), ("Cannot get center of an empty or incomplete way"));
    m2::PointD result;
    for (OsmNode const * n : m_nodes)
      result += n->GetCenter();
    return result / m_nodes.size();
  }

  bool OsmWay::BuildArea()
  {
    m_area = m2::RegionD();
    if (!IsIncomplete() && IsClosed())
    {
      for (size_t i = 0; i < m_nodes.size() - 1; i++)
        m_area.AddPoint(m_nodes[i]->Coords());
    }
    return m_area.IsValid();
  }

  bool OsmWay::IsIncomplete() const
  {
    for (OsmNode const * n : m_nodes)
      if (n->IsIncomplete())
        return true;
    return false;
  }

  m2::PointD const OsmRelation::GetCenter() const
  {
    CHECK(IsMultipolygon(), ("OsmRelation::GetCenter is applicable only to multipolygons"));
    CHECK(!m_members.empty() && !IsIncomplete(), ("Cannot get center of an empty or incomplete relation"));
    // TODO: return point inside m_area?
    m2::PointD result;
    for (OsmMember const & m : m_members)
      result += m.first->GetCenter();
    return result / m_members.size();
  }

  bool OsmRelation::IsIncomplete() const
  {
    for (OsmMember const & m : m_members)
      if (m.first->IsIncomplete())
        return true;
    return false;
  }

  bool OsmRelation::BuildArea()
  {
    // todo
    return false;
  }

  void OsmData::Add(OsmElement * element)
  {
    if (element->Type() == OsmType::NODE)
      m_nodes.emplace(element->Id(), *dynamic_cast<OsmNode*>(element));
    else if (element->Type() == OsmType::WAY)
      m_ways.emplace(element->Id(), *dynamic_cast<OsmWay*>(element));
    else if (element->Type() == OsmType::RELATION)
      m_relations.emplace(element->Id(), *dynamic_cast<OsmRelation*>(element));
  }

  bool OsmData::BuildAreas()
  {
    bool result = false;
    for (auto & w : m_ways)
      result = result || w.second.BuildArea();
    for (auto & r : m_relations)
      if (r.second.IsMultipolygon())
        result = result || r.second.BuildArea();
    return result;
  }

  void replace_all(string & str, string const & from, string const & to)
  {
    size_t pos = str.find(from);
    while(pos != string::npos)
    {
      str.replace(pos, from.length(), to);
      pos = str.find(from, pos + to.length());
    }
  }

  string & screen_xml(string & str)
  {
    replace_all(str, "&", "&amp;");
    replace_all(str, "<", "&lt;");
    replace_all(str, ">", "&gt;");
    replace_all(str, "\"", "&quot;");
    return str;
  }

  void write_tags(ostream & oss, OsmTags const & tags)
  {
    for(pair<string, string> const & tag : tags)
    {
      if (tag.second.length())
      {
        string skey(tag.first);
        string svalue(tag.second);
        oss << "<tag k=\"" << screen_xml(skey) << "\" v=\"" << screen_xml(svalue) << "\" />";
      }
    }
  }

  void OsmData::InnerXML(ostream & oss, OsmId changeset) const
  {
    for (pair<OsmId, OsmNode> const & pn : m_nodes)
    {
      OsmNode const & n = pn.second;
      oss << "<node id=\"" << n.Id() << "\" version=\"" << n.Version();
      if (changeset > 0)
        oss << "\" changeset=\"" << changeset;
      if (!n.IsIncomplete())
        oss << "\" lat=\"" << n.Coords().y << "\" lon=\"" << n.Coords().x;
      oss << "\">";
      write_tags(oss, n.Tags());
      oss << "</node>";
    }
    for (pair<OsmId, OsmWay> const & pw : m_ways)
    {
      OsmWay const & w = pw.second;
      oss << "<way id=\"" << w.Id() << "\" version=\"" << w.Version();
      if (changeset > 0)
        oss << "\" changeset=\"" << changeset;
      oss << "\">";
      write_tags(oss, w.Tags());
      for(OsmNode const * n : w.Nodes())
        oss << "<nd ref=\"" << n->Id() << "\" />";
      oss << "</way>";
    }
    for (pair<OsmId, OsmRelation> const & pr : m_relations)
    {
      OsmRelation const & r = pr.second;
      oss << "<relation id=\"" << r.Id() << "\" version=\"" << r.Version();
      if (changeset > 0)
        oss << "\" changeset=\"" << changeset;
      oss << "\">";
      write_tags(oss, r.Tags());
      for(OsmMember const & m : r.Members())
      {
        oss << "<member type=\"";
        if (m.first->Type() == OsmType::NODE)
          oss << "node";
        else if( m.first->Type() == OsmType::WAY)
          oss << "way";
        else if( m.first->Type() == OsmType::RELATION)
          oss << "relation";
        string role(m.second);
        oss << "\" ref=\"" << m.first->Id() << "\" role=\"" << role << "\" />";
      }
      oss << "</relation>";
    }
  }

  void OsmData::XML(ostream & oss) const
  {
    //oss << "<?xml charset=\"utf-8\">";
    oss << "<osm version=\"0.6\" generator=\"MAPS.MS\">";
    InnerXML(oss);
    oss << "</osm>";
  }

  vector<OsmNode> OsmData::GetNodes() const
  {
    vector<OsmNode> result;
    for (pair<OsmId, OsmNode> const & n : m_nodes)
      result.push_back(n.second);
    return move(result);
  }

  vector<OsmWay> OsmData::GetWays() const
  {
    vector<OsmWay> result;
    for (pair<OsmId, OsmWay> const & w : m_ways)
      result.push_back(w.second);
    return move(result);
  }

  vector<OsmRelation> OsmData::GetRelations() const
  {
    vector<OsmRelation> result;
    for (pair<OsmId, OsmRelation> const & r : m_relations)
      result.push_back(r.second);
    return move(result);
  }

  vector<OsmArea> OsmData::GetAreas() const
  {
    vector<OsmArea> result;
    // todo
    return move(result);
  }

  vector<OsmElement *> OsmData::GetElements(OsmType type) const
  {
    vector<OsmElement *> result;
    if (type == OsmType::NODE)
      for (OsmNode n : GetNodes())
        result.push_back(&n);
    /*
    if( type == OsmType::AREA)
      for (OsmArea *a : GetAreas())
        result.push_back(a);
        */
    return move(result);
  }

  void OsmChange::XML(ostream & oss, OsmId changeset) const
  {
    CHECK(changeset > 0, ("Changeset ID is needed for XML"));
    //oss << "<?xml charset=\"utf-8\">";
    oss << "<osmChange version=\"0.6\" generator=\"MAPS.MS\">";
    oss << "<create>";
    m_created.InnerXML(oss, changeset);
    oss << "</create><modify>";
    m_modified.InnerXML(oss, changeset);
    oss << "</modify><delete>";
    m_deleted.InnerXML(oss, changeset);
    oss << "</delete>";
    oss << "</osmChange>";
  }

  void OsmChange::ChangesetXML(ostream & oss) const
  {
    //oss << "<?xml charset=\"utf-8\">";
    oss << "<osm><changeset>";
    OsmTags ctags;
    ctags["created_by"] = "MAPS.ME";
    if (m_comment.length())
      ctags["comment"] = m_comment;
    write_tags(oss, ctags);
    oss << "</changeset></osm>";
  }

  bool OsmParser::Push(string const & tagName)
  {
    m_tagName = "";
    if (tagName == "node")
    {
      m_node = OsmNode(0);
      m_element = &m_node;
    }
    else if (tagName == "way")
    {
      m_way = OsmWay(0);
      m_element = &m_way;
    }
    else if (tagName == "relation")
    {
      m_relation = OsmRelation(0);
      m_element = &m_relation;
    }
    else
      m_tagName = tagName;
    return true;
  }

  void OsmParser::Pop(string const & tagName)
  {
    if (tagName == "node")
      (dynamic_cast<OsmNode *>(m_element))->Coords(m_coords);
    if (tagName == "node" || tagName == "way" || tagName == "relation")
    {
      m_data.Add(m_element);
      m_tagName = "";
      m_key = "";
      m_value = "";
      m_ref_type = "";
      m_coords = m2::PointD::Zero();
    }
    else if (tagName == "tag" && m_key.length() && m_value.length())
      m_element->SetValue(m_key, m_value);
    else if (tagName == "nd")
      (dynamic_cast<OsmWay *>(m_element))->Add(dynamic_cast<OsmNode const *>(Find()));
    else if (tagName == "member")
      (dynamic_cast<OsmRelation *>(m_element))->Add(Find(), m_ref_role);
  }

  OsmElement const * OsmParser::Find()
  {
    if (m_ref_type == "node")
    {
      if (!m_data.HasNode(m_ref))
      {
        OsmNode nd(m_ref);
        m_data.Add(&nd);
      }
      return m_data.GetNode(m_ref);
    }
    else if (m_ref_type == "way")
    {
      if (!m_data.HasWay(m_ref))
      {
        OsmWay way(m_ref);
        m_data.Add(&way);
      }
      return m_data.GetWay(m_ref);
    }
    else if (m_ref_type == "relation")
    {
      if (!m_data.HasRelation(m_ref))
      {
        OsmRelation rel(m_ref);
        m_data.Add(&rel);
      }
      return m_data.GetRelation(m_ref);
    }
    CHECK(false, ("Incorrect ref type", m_ref, m_ref_type));
    return m_element;
  }

  void OsmParser::AddAttr(string const & attr, string const & value)
  {
    if (m_tagName == "")
    {
      // element attribute
      if (attr == "id")
        strings::to_int64(value, m_element->m_id);
      else if (attr == "version")
        strings::to_uint32(value, m_element->m_version);
      else if (attr == "lat" && m_element->Type() == OsmType::NODE)
        strings::to_double(value, m_coords.y);
      else if (attr == "lon" && m_element->Type() == OsmType::NODE)
        strings::to_double(value, m_coords.x);
    }
    else if (m_tagName == "nd" && attr == "ref" && m_element->Type() == OsmType::WAY)
    {
      m_ref_type = "node";
      strings::to_int64(value, m_ref);
    }
    else if (m_tagName == "member")
    {
      if (attr == "type")
        m_ref_type = value;
      else if (attr == "ref")
        strings::to_int64(value, m_ref);
      else if( attr == "role")
        m_ref_role = value;
    }
    else if (m_tagName == "tag")
    {
      if (attr == "k")
        m_key = value;
      else if (attr == "v")
        m_value = value;
    }
  }
}
