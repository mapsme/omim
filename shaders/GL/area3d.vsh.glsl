attribute vec3 a_position;
attribute vec3 a_normal;
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

varying float v_intensity;

const vec4 kNormalizedLightDir = vec4(0.3162, 0.0, 0.9486, 0.0);

void main()
{
  vec4 pos = vec4(a_position, 1.0) * u_modelView;
  
  vec4 normal = vec4(a_position + a_normal, 1.0) * u_modelView;
  normal.xyw = (normal * u_projection).xyw;
  normal.z = normal.z * u_zScale;

  pos.xyw = (pos * u_projection).xyw;
  pos.z = a_position.z * u_zScale;

  vec4 normDir = normal - pos;
  if (dot(normDir, normDir) != 0.0)
    v_intensity = max(0.0, -dot(kNormalizedLightDir, normalize(normDir)));
  else
    v_intensity = 0.0;

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
