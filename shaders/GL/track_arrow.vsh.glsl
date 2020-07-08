attribute vec2 a_colorTexCoord;
attribute vec3 a_position;
attribute vec2 a_normal;

uniform mat4 u_modelView;
uniform mat4 u_projection;
uniform mat4 u_pivotTransform;

varying vec2 v_symbolTexCoord;

void main()
{
  float displacement = length(a_normal.xy);
  vec2 transformedAxisPos = (vec4(a_position.xy, 0.0, 1.0) * u_modelView).xy;
  if (displacement != 0.0)
  {
    transformedAxisPos = calcLineTransformedAxisPos(transformedAxisPos, a_position.xy + a_normal.xy,
                                                    u_modelView, displacement);
  }

  v_symbolTexCoord = a_colorTexCoord;

  vec4 pos = vec4(transformedAxisPos, a_position.z, 1.0) * u_projection;
  gl_Position = applyPivotTransform(pos, u_pivotTransform, 0.0);
}
