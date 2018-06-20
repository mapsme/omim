#include "shaders/gl_program_params.hpp"

#include "drape/uniform_value.hpp"

#include "base/assert.hpp"

#include <string>

namespace gpu
{
namespace
{
struct UniformsGuard
{
  template <typename ParamsType>
  explicit UniformsGuard(ref_ptr<dp::GpuProgram> program, ParamsType const &)
    : m_program(program)
  {
    ASSERT_EQUAL(ParamsType::GetName(), ProgramParams::GetBoundParamsName(program),
                 ("Mismatched program and parameters"));
  }

  ~UniformsGuard()
  {
    auto const uniformsCount = m_program->GetNumericUniformsCount();
    CHECK_EQUAL(m_counter, uniformsCount, ("Not all numeric uniforms are set up"));
  }

  ref_ptr<dp::GpuProgram> m_program;
  uint32_t m_counter = 0;
};

class Parameter
{
public:
  template<typename ParamType>
  static void CheckApply(UniformsGuard & guard, std::string const & name, ParamType const & t)
  {
    if (Apply(guard.m_program, name, t))
      guard.m_counter++;
  }

private:
  static bool Apply(ref_ptr<dp::GpuProgram> program, std::string const & name, glsl::mat4 const & m)
  {
    auto const location = program->GetUniformLocation(name);
    if (location < 0)
      return false;

    ASSERT_EQUAL(program->GetUniformType(name), gl_const::GLFloatMat4, ());
    dp::UniformValue::ApplyRaw(location, m);
    return true;
  }

  static bool Apply(ref_ptr<dp::GpuProgram> program, std::string const & name, float f)
  {
    auto const location = program->GetUniformLocation(name);
    if (location < 0)
      return false;

    ASSERT_EQUAL(program->GetUniformType(name), gl_const::GLFloatType, ());
    dp::UniformValue::ApplyRaw(location, f);
    return true;
  }

  static bool Apply(ref_ptr<dp::GpuProgram> program, std::string const & name, glsl::vec2 const & v)
  {
    auto const location = program->GetUniformLocation(name);
    if (location < 0)
      return false;

    ASSERT_EQUAL(program->GetUniformType(name), gl_const::GLFloatVec2, ());
    dp::UniformValue::ApplyRaw(location, v);
    return true;
  }

  static bool Apply(ref_ptr<dp::GpuProgram> program, std::string const & name, glsl::vec3 const & v)
  {
    auto const location = program->GetUniformLocation(name);
    if (location < 0)
      return false;

    ASSERT_EQUAL(program->GetUniformType(name), gl_const::GLFloatVec3, ());
    dp::UniformValue::ApplyRaw(location, v);
    return true;
  }

  static bool Apply(ref_ptr<dp::GpuProgram> program, std::string const & name, glsl::vec4 const & v)
  {
    auto const location = program->GetUniformLocation(name);
    if (location < 0)
      return false;

    ASSERT_EQUAL(program->GetUniformType(name), gl_const::GLFloatVec4, (program->GetName()));
    dp::UniformValue::ApplyRaw(location, v);
    return true;
  }
};
}  // namespace

void GLProgramParamsSetter::Apply(ref_ptr<dp::GpuProgram> program, MapProgramParams const & params)
{
  UniformsGuard guard(program, params);

  Parameter::CheckApply(guard, "u_modelView", params.m_modelView);
  Parameter::CheckApply(guard, "u_projection", params.m_projection);
  Parameter::CheckApply(guard, "u_pivotTransform", params.m_pivotTransform);
  Parameter::CheckApply(guard, "u_opacity", params.m_opacity);
  Parameter::CheckApply(guard, "u_zScale", params.m_zScale);
  Parameter::CheckApply(guard, "u_interpolation", params.m_interpolation);
  Parameter::CheckApply(guard, "u_isOutlinePass", params.m_isOutlinePass);
  Parameter::CheckApply(guard, "u_contrastGamma", params.m_contrastGamma);
}

void GLProgramParamsSetter::Apply(ref_ptr<dp::GpuProgram> program, RouteProgramParams const & params)
{
  UniformsGuard guard(program, params);

  Parameter::CheckApply(guard, "u_modelView", params.m_modelView);
  Parameter::CheckApply(guard, "u_projection", params.m_projection);
  Parameter::CheckApply(guard, "u_pivotTransform", params.m_pivotTransform);
  Parameter::CheckApply(guard, "u_routeParams", params.m_routeParams);
  Parameter::CheckApply(guard, "u_color", params.m_color);
  Parameter::CheckApply(guard, "u_maskColor", params.m_maskColor);
  Parameter::CheckApply(guard, "u_outlineColor", params.m_outlineColor);
  Parameter::CheckApply(guard, "u_pattern", params.m_pattern);
  Parameter::CheckApply(guard, "u_angleCosSin", params.m_angleCosSin);
  Parameter::CheckApply(guard, "u_arrowHalfWidth", params.m_arrowHalfWidth);
  Parameter::CheckApply(guard, "u_opacity", params.m_opacity);
}

void GLProgramParamsSetter::Apply(ref_ptr<dp::GpuProgram> program, TrafficProgramParams const & params)
{
  UniformsGuard guard(program, params);

  Parameter::CheckApply(guard, "u_modelView", params.m_modelView);
  Parameter::CheckApply(guard, "u_projection", params.m_projection);
  Parameter::CheckApply(guard, "u_pivotTransform", params.m_pivotTransform);
  Parameter::CheckApply(guard, "u_trafficParams", params.m_trafficParams);
  Parameter::CheckApply(guard, "u_outlineColor", params.m_outlineColor);
  Parameter::CheckApply(guard, "u_outline", params.m_outline);
  Parameter::CheckApply(guard, "u_lightArrowColor", params.m_lightArrowColor);
  Parameter::CheckApply(guard, "u_opacity", params.m_opacity);
  Parameter::CheckApply(guard, "u_darkArrowColor", params.m_darkArrowColor);
}

void GLProgramParamsSetter::Apply(ref_ptr<dp::GpuProgram> program, TransitProgramParams const & params)
{
  UniformsGuard guard(program, params);

  Parameter::CheckApply(guard, "u_modelView", params.m_modelView);
  Parameter::CheckApply(guard, "u_projection", params.m_projection);
  Parameter::CheckApply(guard, "u_pivotTransform", params.m_pivotTransform);
  Parameter::CheckApply(guard, "u_params", params.m_params);
  Parameter::CheckApply(guard, "u_lineHalfWidth", params.m_lineHalfWidth);
}

void GLProgramParamsSetter::Apply(ref_ptr<dp::GpuProgram> program, GuiProgramParams const & params)
{
  UniformsGuard guard(program, params);

  Parameter::CheckApply(guard, "u_modelView", params.m_modelView);
  Parameter::CheckApply(guard, "u_projection", params.m_projection);
  Parameter::CheckApply(guard, "u_contrastGamma", params.m_contrastGamma);
  Parameter::CheckApply(guard, "u_position", params.m_position);
  Parameter::CheckApply(guard, "u_isOutlinePass", params.m_isOutlinePass);
  Parameter::CheckApply(guard, "u_opacity", params.m_opacity);
  Parameter::CheckApply(guard, "u_length", params.m_length);
}

void GLProgramParamsSetter::Apply(ref_ptr<dp::GpuProgram> program, AccuracyProgramParams const & params)
{
  UniformsGuard guard(program, params);

  Parameter::CheckApply(guard, "u_modelView", params.m_modelView);
  Parameter::CheckApply(guard, "u_projection", params.m_projection);
  Parameter::CheckApply(guard, "u_pivotTransform", params.m_pivotTransform);
  Parameter::CheckApply(guard, "u_position", params.m_position);
  Parameter::CheckApply(guard, "u_accuracy", params.m_accuracy);
  Parameter::CheckApply(guard, "u_zScale", params.m_zScale);
  Parameter::CheckApply(guard, "u_opacity", params.m_opacity);
}

void GLProgramParamsSetter::Apply(ref_ptr<dp::GpuProgram> program, MyPositionProgramParams const & params)
{
  UniformsGuard guard(program, params);

  Parameter::CheckApply(guard, "u_modelView", params.m_modelView);
  Parameter::CheckApply(guard, "u_projection", params.m_projection);
  Parameter::CheckApply(guard, "u_pivotTransform", params.m_pivotTransform);
  Parameter::CheckApply(guard, "u_position", params.m_position);
  Parameter::CheckApply(guard, "u_azimut", params.m_azimut);
  Parameter::CheckApply(guard, "u_opacity", params.m_opacity);
}

void GLProgramParamsSetter::Apply(ref_ptr<dp::GpuProgram> program, Arrow3dProgramParams const & params)
{
  UniformsGuard guard(program, params);

  Parameter::CheckApply(guard, "u_transform", params.m_transform);
  Parameter::CheckApply(guard, "u_color", params.m_color);
}

void GLProgramParamsSetter::Apply(ref_ptr<dp::GpuProgram> program, DebugRectProgramParams const & params)
{
  UniformsGuard guard(program, params);

  Parameter::CheckApply(guard, "u_color", params.m_color);
}

void GLProgramParamsSetter::Apply(ref_ptr<dp::GpuProgram> program, ScreenQuadProgramParams const & params)
{
  UniformsGuard guard(program, params);

  Parameter::CheckApply(guard, "u_opacity", params.m_opacity);
}

void GLProgramParamsSetter::Apply(ref_ptr<dp::GpuProgram> program, SMAAProgramParams const & params)
{
  UniformsGuard guard(program, params);

  Parameter::CheckApply(guard, "u_framebufferMetrics", params.m_framebufferMetrics);
}
}  // namespace gpu
