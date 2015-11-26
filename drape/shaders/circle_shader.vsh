attribute vec3 a_position;
attribute vec3 a_normal;
attribute vec2 a_colorTexCoords;

uniform mat4 modelView;
uniform mat4 projection;
uniform mat4 pivotTransform;

varying vec3 v_radius;
varying vec2 v_colorTexCoords;

void main(void)
{
  vec4 pos = (vec4(a_normal.xy, 0, 0) + vec4(a_position, 1) * modelView) * projection;
  float w = pos.w;
  pos.xyw = (pivotTransform * pos).xyw;
  pos.z *= pos.w / w;
  gl_Position = pos;
  v_colorTexCoords = a_colorTexCoords;
  v_radius = a_normal;
}
