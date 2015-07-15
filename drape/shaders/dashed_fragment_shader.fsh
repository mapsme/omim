in vec2 v_colorTexCoord;
in vec2 v_maskTexCoord;

out vec4 v_FragColor;

uniform sampler2D u_colorTex;
uniform sampler2D u_maskTex;
uniform float u_opacity;

void main(void)
{
  vec4 color = texture(u_colorTex, v_colorTexCoord);
  float mask = texture(u_maskTex, v_maskTexCoord).r;
  color.a = color.a * mask * u_opacity;
  v_FragColor =  color;
}
