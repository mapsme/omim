attribute vec3 a_position;
attribute vec2 a_colorTexCoords;
attribute vec2 a_highlightingTexCoords;

uniform mat4 u_modelView;
uniform mat4 u_projection;
uniform mat4 u_pivotTransform;
uniform float u_zScale;

#ifdef ENABLE_VTF
uniform sampler2D u_colorTex;
uniform float u_opacity;

varying LOW_P vec4 v_color;
#else
varying vec4 v_texCoords;
#endif

void main()
{
  vec4 pos = vec4(a_position, 1.0) * u_modelView;
  pos.xyw = (pos * u_projection).xyw;
  pos.z = a_position.z * u_zScale;
  gl_Position = u_pivotTransform * pos;
#ifdef VULKAN
  gl_Position.y = -gl_Position.y;
  gl_Position.z = (gl_Position.z  + gl_Position.w) * 0.5;
#endif

#ifdef ENABLE_VTF
  v_color = vec4(texture2D(u_colorTex, a_colorTexCoords).rgb, u_opacity);
  vec4 highlighting = texture2D(u_colorTex, a_highlightingTexCoords);
  v_color.rgb = mix(v_color.rgb, highlighting.rgb, highlighting.a);
#else
  v_texCoords = vec4(a_colorTexCoords, a_highlightingTexCoords);
#endif
}
