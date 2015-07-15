in vec3 a_position;
in vec2 a_normal;
in vec2 a_colorTexCoords;
in float a_animate;

uniform mat4 modelView;
uniform mat4 projection;
uniform float u_interpolationT;

out vec2 v_colorTexCoords;

void main(void)
{
  vec2 normal = a_normal;
  if (a_animate > 0.0)
    normal = u_interpolationT * normal;
  gl_Position = (vec4(normal, 0, 0) + vec4(a_position, 1) * modelView) * projection;
  v_colorTexCoords = a_colorTexCoords;
}
