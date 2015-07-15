in vec3 a_position;
in vec2 a_normal;
in vec2 a_colorTexCoords;

uniform mat4 modelView;
uniform mat4 projection;

out vec2 v_colorTexCoords;

void main(void)
{
  gl_Position = (vec4(a_normal, 0, 0) + vec4(a_position, 1) * modelView) * projection;
  v_colorTexCoords = a_colorTexCoords;
}
