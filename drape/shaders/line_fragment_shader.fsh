in vec4 v_colorTexCoord;
in vec4 v_halfLength;

out vec4 v_FragColor;

uniform sampler2D u_colorTex;
uniform float u_opacity;

const float aaPixelsCount = 2.0;

void main(void)
{
  vec4 color;
  float currentW = abs(v_halfLength.x);
  if (currentW <= v_halfLength.z)
  {
	color = texture(u_colorTex, v_colorTexCoord.zw);
	gl_FragDepth = v_halfLength.w;
  }
  else
  {
	color = texture(u_colorTex, v_colorTexCoord.xy);
	gl_FragDepth = gl_FragCoord.z;
  }
  color.a *= u_opacity;

  float diff = v_halfLength.y - currentW;
  color.a *= mix(0.3, 1.0, clamp(diff / aaPixelsCount, 0.0, 1.0));
  v_FragColor = color;
}
