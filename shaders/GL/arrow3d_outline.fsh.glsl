varying float v_intensity;

#ifdef SAMSUNG_GOOGLE_NEXUS
uniform sampler2D u_colorTex;
#endif

uniform vec4 u_color;

void main()
{
  vec4 resColor = vec4(u_color.rgb, u_color.a * smoothstep(0.7, 1.0, v_intensity));
  gl_FragColor = samsungGoogleNexusWorkaround(resColor);
}
