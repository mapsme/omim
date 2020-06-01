uniform float u_opacity;
uniform sampler2D u_colorTex;

varying vec2 v_symbolTexCoord;

void main()
{
  vec4 finalColor = texture2D(u_colorTex, v_symbolTexCoord);
  finalColor.a *= u_opacity;
  gl_FragColor = finalColor;
}
