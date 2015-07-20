in vec4 a_position;
in vec4 a_normal;
in vec4 a_colorTexCoord;

uniform mat4 modelView;
uniform mat4 projection;

out vec4 v_colorTexCoord;
out vec4 v_halfLength;

void main(void)
{
  vec2 normal = a_normal.xy;
  float halfWidth = length(normal);
  vec2 transformedAxisPos = (vec4(a_position.xy, 0.0, 1.0) * modelView).xy;
  if (halfWidth != 0.0)
  {
    vec4 glbShiftPos = vec4(a_position.xy + normal, 0.0, 1.0);
    vec2 shiftPos = (glbShiftPos * modelView).xy;
    transformedAxisPos = transformedAxisPos + normalize(shiftPos - transformedAxisPos) * halfWidth;
  }

  vec2 depth = (vec4(0.0, 0.0, a_position.w, 1.0) * projection).zw;
  float innerDepth = 0.5 * depth.x / depth.y + 0.5;
  v_colorTexCoord = a_colorTexCoord;
  v_halfLength = vec4(sign(a_normal.z) * halfWidth, abs(a_normal.z), a_normal.w, innerDepth);
  gl_Position = vec4(transformedAxisPos, a_position.z, 1.0) * projection;
}
