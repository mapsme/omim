varying vec2 v_colorTexCoord;
varying vec2 v_halfLength;
varying vec2 v_maskTexCoord;

uniform sampler2D u_colorTex;
uniform sampler2D u_maskTex;
uniform float u_opacity;

const float aaPixelsCount = 2.5;

void main(void)
{
  vec4 color = texture2D(u_colorTex, v_colorTexCoord);
  vec4 mask = texture2D(u_maskTex, v_maskTexCoord);
  mask.a = 0.0;
  color.a = color.a * u_opacity;
  
  float currentW = abs(v_halfLength.x);
  float diff = v_halfLength.y - currentW;
  color.a *= mix(0.0, 1.0, clamp(diff / aaPixelsCount, 0.0, 1.0));
  color.rgb = mix(color.rgb, mask.rgb, mask.a);

  gl_FragColor = color;
}
