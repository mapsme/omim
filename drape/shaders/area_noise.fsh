uniform sampler2D u_colorTex;
uniform float u_opacity;

varying vec2 v_position;
varying vec2 v_colorTexCoords;

float randomNoise(vec2 p)
{
  return fract(6791.*sin(47.*p.x+p.y*9973.));
}

void main(void)
{
  vec4 finalColor = texture2D(u_colorTex, v_colorTexCoords);
  finalColor.a *= u_opacity;
  vec2 position = v_colorTexCoords;
  float noise = (randomNoise(v_position) + 1.0) / 2.0;
  finalColor.rgb *= clamp(noise, 0.88, 1.0);
  gl_FragColor = finalColor;
}


