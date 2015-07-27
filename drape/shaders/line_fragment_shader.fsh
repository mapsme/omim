in vec4 v_colorTexCoord;
in vec4 v_halfLength;

out vec4 v_FragColor;

uniform sampler2D u_colorTex;
uniform float u_opacity;

void main(void)
{
  vec4 finalColor = texture(u_colorTex, v_colorTexCoord.zw);
  float depth = v_halfLength.w;

  float currentW = abs(v_halfLength.x);
  float diff = currentW - v_halfLength.z;
  if (diff > 0.0)
  {
    finalColor = texture(u_colorTex, v_colorTexCoord.xy);
    float outlineWidth = v_halfLength.y - v_halfLength.z;
    float fadeOutFactor = smoothstep(1.0, 0.0, diff / outlineWidth);
    finalColor.a *= fadeOutFactor;
    depth = gl_FragCoord.z;
  }

  finalColor.a *= u_opacity;
  v_FragColor = finalColor;
  gl_FragDepth = depth;
}
