in vec2 v_length;

out vec4 v_FragColor;

uniform vec4 u_color;
uniform float u_clipLength;

void main(void)
{
  vec4 color = u_color;
  if (v_length.x < u_clipLength)
    color.a = 0.0;

  v_FragColor = color;
}
