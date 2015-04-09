#include "osm_entity.hpp"
#include "../std/iostream.hpp"

void edit::DummyOsmParser::AddAttr(string const & key, string const & value)
{

  if (m_current)
  {
    if (key == "id")
      m_current->id = stol(value);
    else if (key == "version")
      m_current->version = stoi(value);
    else if (key == "user")
      m_current->user = value;
    else if (key == "uid")
      m_current->uid = stoi(value);
    else if (key == "changeset")
      m_current->changeset = stoi(value);
    else if (key == "visible")
      m_current->visible = value;
    else if (key == "timestamp")
      m_current->timestamp = value;

    else if (key == "lon")
      m_current->lon = stod(value);
    else if (key == "lat")
      m_current->lat = stod(value);

    else if (key == "k")
      m_tmpK = value;
    else if (key == "v")
      m_tmpV = value;
    else if (key == "type")
      m_tmpType = value;
    else if (key == "role")
      m_tmpRole = value;
    else if (key == "ref")
      m_tmpRef = stol(value);
    else
      cout << (m_subelement ? "\t" : "") << "Key: " << key << " Value: " << value << endl;
  }
}

bool edit::DummyOsmParser::Push(string const & tagName)
{
  ++m_depth;

  m_tagName = tagName;

  if (m_depth == 1)
  {
    m_current = 0;
  }
  else if (m_depth == 2)
  {
    m_entityList.emplace_back();
    m_current = &m_entityList.back();
    m_current->type = tagName;
  }
  else
  {
    m_subelement = true;
  }
  return true;
}

void edit::DummyOsmParser::Pop(string const &)
{
  --m_depth;

  if (m_depth >= 2)
  {
    m_subelement = false;
    if (m_tagName == "nd")
      m_current->nds.push_back(m_tmpRef);
    else if (m_tagName == "member")
      m_current->members.emplace_back(m_tmpType, m_tmpRef, m_tmpRole);
    else if (m_tagName == "tag")
      m_current->tags.emplace(m_tmpK, m_tmpV);
  }
}