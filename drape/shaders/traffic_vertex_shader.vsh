attribute vec3 a_position;
attribute vec4 a_normal;
attribute vec2 a_colorTexCoord;

uniform mat4 modelView;
uniform mat4 projection;
uniform mat4 pivotTransform;

uniform vec4 u_trafficParams;

varying vec2 v_colorTexCoord;
varying vec2 v_maskTexCoord;
varying vec2 v_halfLength;

void main(void)
{
  vec2 normal = a_normal.xy;
  float halfWidth = length(normal);
  vec2 transformedAxisPos = (vec4(a_position.xy, 0.0, 1.0) * modelView).xy;
  if (u_trafficParams.x != 0.0 && halfWidth != 0.0)
  {
    vec2 norm = normal * u_trafficParams.x;
    halfWidth = length(norm);

    vec4 glbShiftPos = vec4(a_position.xy + norm, 0.0, 1.0);
    vec2 shiftPos = (glbShiftPos * modelView).xy;
    transformedAxisPos = transformedAxisPos + normalize(shiftPos - transformedAxisPos) * halfWidth;
  }

  float uOffset = min(length(vec4(1, 0, 0, 0) * modelView) * a_normal.w, 1.0);
  v_colorTexCoord = a_colorTexCoord;
  v_maskTexCoord = vec2(u_trafficParams.y + uOffset * u_trafficParams.z, u_trafficParams.w);
  v_halfLength = vec2(sign(a_normal.z), abs(a_normal.z)) * halfWidth;
  vec4 pos = vec4(transformedAxisPos, a_position.z, 1.0) * projection;
  float w = pos.w;
  pos.xyw = (pivotTransform * vec4(pos.xy, 0.0, w)).xyw;
  pos.z *= pos.w / w;
  gl_Position = pos;
}
