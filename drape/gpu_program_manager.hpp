#pragma once

#include "drape/pointers.hpp"
#include "drape/gpu_program.hpp"
#include "drape/shader.hpp"

#include <map>
#include <boost/noncopyable.hpp>

namespace dp
{

class GpuProgramManager : public boost::noncopyable
{
public:
  ~GpuProgramManager();

  void Init();

  ref_ptr<GpuProgram> GetProgram(int index);

private:
  ref_ptr<Shader> GetShader(int index, std::string const & source, Shader::Type t);

private:
  typedef std::map<int, drape_ptr<GpuProgram> > program_map_t;
  typedef std::map<int, drape_ptr<Shader> > shader_map_t;
  program_map_t m_programs;
  shader_map_t m_shaders;
  std::string m_globalDefines;
};

} // namespace dp
