uniform vec4 u_color;

out vec4 v_FragColor;

void main(void)
{
  if (u_color.a < 0.1)
    discard;

  v_FragColor = u_color;
}
