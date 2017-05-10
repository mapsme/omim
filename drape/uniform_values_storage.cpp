#include "drape/uniform_values_storage.hpp"

#include "base/assert.hpp"

#include <algorithm>
#include <functional>

namespace dp
{

void UniformValuesStorage::SetIntValue(std::string const & name, int32_t v)
{
  UniformValue * uniform = findByName(name);
  if (uniform)
    uniform->SetIntValue(v);
  else
    m_uniforms.push_back(UniformValue(name, v));
}

void UniformValuesStorage::SetIntValue(std::string const & name, int32_t v1, int32_t v2)
{
  UniformValue * uniform = findByName(name);
  if (uniform)
    uniform->SetIntValue(v1, v2);
  else
    m_uniforms.push_back(UniformValue(name, v1, v2));
}

void UniformValuesStorage::SetIntValue(std::string const & name, int32_t v1, int32_t v2, int32_t v3)
{
  UniformValue * uniform = findByName(name);
  if (uniform)
    uniform->SetIntValue(v1, v2, v3);
  else
    m_uniforms.push_back(UniformValue(name, v1, v2, v3));
}

void UniformValuesStorage::SetIntValue(std::string const & name, int32_t v1, int32_t v2, int32_t v3, int32_t v4)
{
  UniformValue * uniform = findByName(name);
  if (uniform)
    uniform->SetIntValue(v1, v2, v3, v4);
  else
    m_uniforms.push_back(UniformValue(name, v1, v2, v3, v4));
}

void UniformValuesStorage::SetFloatValue(std::string const & name, float v)
{
  UniformValue * uniform = findByName(name);
  if (uniform)
    uniform->SetFloatValue(v);
  else
    m_uniforms.push_back(UniformValue(name, v));
}

void UniformValuesStorage::SetFloatValue(std::string const & name, float v1, float v2)
{
  UniformValue * uniform = findByName(name);
  if (uniform)
    uniform->SetFloatValue(v1, v2);
  else
    m_uniforms.push_back(UniformValue(name, v1, v2));
}

void UniformValuesStorage::SetFloatValue(std::string const & name, float v1, float v2, float v3)
{
  UniformValue * uniform = findByName(name);
  if (uniform)
    uniform->SetFloatValue(v1, v2, v3);
  else
    m_uniforms.push_back(UniformValue(name, v1, v2, v3));
}

void UniformValuesStorage::SetFloatValue(std::string const & name, float v1, float v2, float v3, float v4)
{
  UniformValue * uniform = findByName(name);
  if (uniform)
    uniform->SetFloatValue(v1, v2, v3, v4);
  else
    m_uniforms.push_back(UniformValue(name, v1, v2, v3, v4));
}

void UniformValuesStorage::SetMatrix4x4Value(std::string const & name, float const * matrixValue)
{
  UniformValue * uniform = findByName(name);
  if (uniform)
    uniform->SetMatrix4x4Value(matrixValue);
  else
    m_uniforms.emplace_back(name, matrixValue);
}

bool UniformValuesStorage::operator< (UniformValuesStorage const & other) const
{
  if (m_uniforms.size() < other.m_uniforms.size())
    return true;

  if (m_uniforms.size() > other.m_uniforms.size())
    return false;

  for (size_t i = 0; i < m_uniforms.size(); ++i)
  {
    if (m_uniforms[i] < other.m_uniforms[i])
      return true;
  }

  return false;
}

namespace
{

bool isNameEqual(std::string const & name, UniformValue const & value)
{
  return name == value.GetName();
}

} // namespace

UniformValue * UniformValuesStorage::findByName(std::string const & name)
{
  uniforms_t::iterator it = std::find_if(m_uniforms.begin(), m_uniforms.end(), std::bind(&isNameEqual, name, std::placeholders::_1));
  if (it == m_uniforms.end())
    return NULL;

  return &(*it);
}

} // namespace dp
