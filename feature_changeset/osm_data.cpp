#include "osm_data.hpp"

namespace osm
{
  m2::PointD OsmWay::GetCenter() const
  {
    m2::PointD result;
    for (OsmNode n : m_nodes)
      result += n.GetCenter();
    return result / m_nodes.size();
  }

  bool OsmWay::BuildArea()
  {
    m_area = m2::RegionD();
    if (!IsIncomplete() && IsClosed())
    {
      for (size_t i = 0; i < m_nodes.size() - 1; i++)
        m_area.AddPoint(m_nodes[i].get().Coords());
    }
    return m_area.IsValid();
  }

  bool OsmWay::IsIncomplete() const
  {
    for (OsmNode n : m_nodes)
      if (n.IsIncomplete())
        return true;
    return false;
  }

  m2::PointD OsmRelation::GetCenter() const
  {
    CHECK(IsMultipolygon(), ("OsmRelation::GetCenter is applicable only to multipolygons"));
    // todo: return point inside m_area
    m2::PointD result;
    for (OsmMember m : m_members)
      result += m.first.get().GetCenter();
    return result / m_members.size();
  }

  bool OsmRelation::IsIncomplete() const
  {
    for (OsmMember m : m_members)
      if (m.first.get().IsIncomplete())
        return true;
    return false;
  }

  bool OsmRelation::BuildArea()
  {
    // todo
    return false;
  }

  void OsmData::Add(OsmElement &element)
  {
    if (element.Type() == OsmType::NODE)
      m_nodes.emplace(element.Id(), std::ref(dynamic_cast<OsmNode&>(element)));
    else if (element.Type() == OsmType::WAY)
      m_ways.emplace(element.Id(), std::ref(dynamic_cast<OsmWay&>(element)));
    else if (element.Type() == OsmType::RELATION)
      m_relations.emplace(element.Id(), std::ref(dynamic_cast<OsmRelation&>(element)));
  }

  bool OsmData::BuildAreas()
  {
    bool result = false;
    for (pair<OsmId, std::reference_wrapper<OsmWay>> w : m_ways)
      result = result || w.second.get().BuildArea();
    for (pair<OsmId, std::reference_wrapper<OsmRelation>> r : m_relations)
      if (r.second.get().IsMultipolygon())
        result = result || r.second.get().BuildArea();
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
    for(pair<string, string> tag : tags)
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
    for (pair<OsmId, OsmNode> pn : m_nodes)
    {
      OsmNode n = pn.second;
      oss << "<node id=\"" << n.m_id << "\" version=\"" << n.m_version;
      if (changeset > 0)
        oss << "\" changeset=\"" << changeset;
      if (!n.IsIncomplete())
        oss << "\" lat=\"" << n.Coords().y << "\" lon=\"" << n.Coords().x;
      oss << "\">";
      write_tags(oss, n.m_tags);
      oss << "</node>";
    }
    for (pair<OsmId, OsmWay> pw : m_ways)
    {
      OsmWay w = pw.second;
      oss << "<way id=\"" << w.m_id << "\" version=\"" << w.m_version;
      if (changeset > 0)
        oss << "\" changeset=\"" << changeset;
      oss << "\">";
      write_tags(oss, w.m_tags);
      for(OsmNode n : w.Nodes())
        oss << "<nd ref=\"" << n.m_id << "\" />";
      oss << "</way>";
    }
    for (pair<OsmId, OsmRelation> pr : m_relations)
    {
      OsmRelation r = pr.second;
      oss << "<relation id=\"" << r.m_id << "\" version=\"" << r.m_version;
      if (changeset > 0)
        oss << "\" changeset=\"" << changeset;
      oss << "\">";
      write_tags(oss, r.m_tags);
      for(OsmMember m : r.Members())
      {
        oss << "<member type=\"";
        if (m.first.get().Type() == OsmType::NODE)
          oss << "node";
        else if( m.first.get().Type() == OsmType::WAY)
          oss << "way";
        else if( m.first.get().Type() == OsmType::RELATION)
          oss << "relation";
        string role(m.second);
        oss << "\" ref=\"" << m.first.get().Id() << "\" role=\"" << role << "\" />";
      }
      oss << "</relation>";
    }
  }

  void OsmData::XML(ostream & oss) const
  {
    oss << "<?xml charset=\"utf-8\">\n<osm version=\"0.6\" generator=\"MAPS.MS\">\n";
    InnerXML(oss);
    oss << "</osm>";
  }

  vector<OsmNodeRef> OsmData::GetNodes() const
  {
    vector<OsmNodeRef> result;
    for (pair<OsmId, OsmNodeRef> n : m_nodes)
      result.push_back(n.second);
    return move(result);
  }

  vector<OsmWayRef> OsmData::GetWays() const
  {
    vector<OsmWayRef> result;
    for (pair<OsmId, OsmWayRef> w : m_ways)
      result.push_back(w.second);
    return move(result);
  }

  vector<OsmRelationRef> OsmData::GetRelations() const
  {
    vector<OsmRelationRef> result;
    for (pair<OsmId, OsmRelationRef> r : m_relations)
      result.push_back(r.second);
    return move(result);
  }

  vector<OsmAreaRef> OsmData::GetAreas() const
  {
    vector<OsmAreaRef> result;
    // todo
    return move(result);
  }

  vector<OsmElementRef> OsmData::GetElements(OsmType type) const
  {
    vector<OsmElementRef> result;
    if( type == OsmType::NODE)
      for (OsmNodeRef n : GetNodes())
        result.push_back(std::ref(dynamic_cast<OsmElement&>(n.get())));
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
    //oss << "<?xml charset=\"utf-8\">\n";
    oss << "<osmChange version=\"0.6\" generator=\"MAPS.MS\">\n";
    oss << "<create>\n";
    m_created.InnerXML(oss, changeset);
    oss << "</create>\n<modify>\n";
    m_modified.InnerXML(oss, changeset);
    oss << "</modify>\n<delete>\n";
    m_deleted.InnerXML(oss, changeset);
    oss << "</delete>\n";
    oss << "</osmChange>";
  }

  void OsmChange::ChangesetXML(ostream & oss) const
  {
    //oss << "<?xml charset=\"utf-8\">\n";
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
    if (tagName == "node")
    {
      OsmNode nd(0);
      m_element = &nd;
    }
    else if (tagName == "way")
    {
      OsmWay w(0);
      m_element = &w;
    }
    else if (tagName == "relation")
    {
      OsmRelation rel(0);
      m_element = &rel;
    }
    else
      m_tagName = tagName;
    return true;
  }

/*  void OsmParser::Pop(string const & tagName)
  {
    if (tagName == "node" || tagName == "way" || tagName == "relation")
    {
      m_data.Add(*m_element);
      m_tagName = "";
      m_key = "";
      m_value = "";
      m_ref_type = "";
    }
    else if (tagName == "tag" && m_key.length() && m_value.length())
      m_element->SetValue(m_key, m_value);
    else if (tagName == "nd")
      m_element->Add(FindRef());
    else if (tagName == "member")
      m_element->Add(FindRef(), m_ref_role);
  }

  OsmElement & OsmParser::FindRef()
  {
    if (m_ref_type == "node")
    {
      if (!m_data.HasNode(m_ref))
      {
        OsmNode nd(m_ref);
        m_data.Add(nd);
      }
      return m_data.GetNode(m_ref);
    }
    else if (m_ref_type == "way")
    {
      if (!m_data.HasWay(m_ref))
      {
        OsmWay way(m_ref);
        m_data.Add(way);
      }
      return m_data.GetWay(m_ref);
    }
    else if (m_ref_type == "relation")
    {
      if (!m_data.HasRelation(m_ref))
      {
        OsmRelation rel(m_ref);
        m_data.Add(rel);
      }
      return m_data.GetRelation(m_ref);
    }
    else
      CHECK(false, ("Incorrect ref type", m_ref, m_ref_type));
  }

  void OsmParser::AddAttr(string const & attr, string const & value)
  {
    if (m_tagName == "")
    {
      // element attribute
      if (attr == "id")
        strings::to_int64(value, m_element->Id());
      else if (attr == "version")
        strings::to_uint32(value, m_element->Version());
      else if (attr == "lat" && m_element->Type() == OsmType::NODE)
        strings::to_double(value, m_element->Coords().y);
      else if (attr == "lon" && m_element->Type() == OsmType::NODE)
        strings::to_double(value, m_element->Coords().x);
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
  } */
}
