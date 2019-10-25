#ifdef ENABLE_VTF
varying LOW_P vec4 v_color;
#else
uniform sampler2D u_colorTex;
uniform float u_opacity;

varying vec4 v_texCoords;
#endif
varying float v_intensity;

void main()
{
#ifdef ENABLE_VTF
  LOW_P vec4 finalColor = v_color;
#else
  LOW_P vec4 finalColor = vec4(texture2D(u_colorTex, v_texCoords.xy).rgb, u_opacity);
  LOW_P vec4 highlighting = texture2D(u_colorTex, v_texCoords.zw);
  finalColor.rgb = mix(finalColor.rgb, highlighting.rgb, highlighting.a);
#endif
  gl_FragColor = vec4((v_intensity * 0.2 + 0.8) * finalColor.rgb, finalColor.a);
}
