#pragma once

#include "drape/uniform_value.hpp"

#include <vector>
#include <string>
#include <functional>

namespace dp
{

class UniformValuesStorage
{
public:
  void SetIntValue(std::string const & name, int32_t v);
  void SetIntValue(std::string const & name, int32_t v1, int32_t v2);
  void SetIntValue(std::string const & name, int32_t v1, int32_t v2, int32_t v3);
  void SetIntValue(std::string const & name, int32_t v1, int32_t v2, int32_t v3, int32_t v4);

  void SetFloatValue(std::string const & name, float v);
  void SetFloatValue(std::string const & name, float v1, float v2);
  void SetFloatValue(std::string const & name, float v1, float v2, float v3);
  void SetFloatValue(std::string const & name, float v1, float v2, float v3, float v4);

  void SetMatrix4x4Value(std::string const & name, float const * matrixValue);

  template<typename TFunctor>
  void ForeachValue(TFunctor & functor) const
  {
    for_each(m_uniforms.begin(), m_uniforms.end(), functor);
  }

  bool operator< (UniformValuesStorage const & other) const;

private:
  UniformValue * findByName(std::string const & name);

private:
  typedef std::vector<UniformValue> uniforms_t;
  uniforms_t m_uniforms;
};

} // namespace dp
