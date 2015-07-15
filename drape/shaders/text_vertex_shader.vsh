in vec4 a_position;
in vec2 a_normal;
in vec2 a_colorTexCoord;
in vec2 a_outlineColorTexCoord;
in vec2 a_maskTexCoord;

uniform mat4 modelView;
uniform mat4 projection;

out vec2 v_colorTexCoord;
out vec2 v_maskTexCoord;
out vec2 v_outlineColorTexCoord;

void main()
{
  gl_Position = (vec4(a_normal, 0, 0) + a_position * modelView) * projection;
  v_colorTexCoord = a_colorTexCoord;
  v_maskTexCoord = a_maskTexCoord;
  v_outlineColorTexCoord = a_outlineColorTexCoord;
}
