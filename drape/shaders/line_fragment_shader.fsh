in vec4 v_colorTexCoord;
in vec4 v_halfLength;

out vec4 v_FragColor;

uniform sampler2D u_colorTex;
uniform float u_opacity;

const float aaPixelsCount = 2.0;
const float outlineBorderSize = 1.0;

void main(void)
{
  vec4 color;
  float currentW = abs(v_halfLength.x);
  float w = currentW - v_halfLength.z;
  if (w <= 0.0)
  {
	color = texture(u_colorTex, v_colorTexCoord.zw);
	gl_FragDepth = v_halfLength.w;
  }
  else if (w <= outlineBorderSize)
  {
	vec4 lineColor = texture(u_colorTex, v_colorTexCoord.zw);
	vec4 outlineColor = texture(u_colorTex, v_colorTexCoord.xy);
	float k = smoothstep(0.0, 1.0, clamp(w / outlineBorderSize, 0.0, 1.0));
	color = mix(lineColor, outlineColor, k);
	gl_FragDepth = gl_FragCoord.z;
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
