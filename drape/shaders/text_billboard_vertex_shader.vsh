attribute vec4 a_position;
attribute vec2 a_normal;
attribute vec2 a_colorTexCoord;
attribute vec2 a_maskTexCoord;

uniform mat4 modelView;
uniform mat4 projection;
uniform mat4 pivotTransform;
uniform float u_isOutlinePass;

#ifdef ENABLE_VTF
uniform sampler2D u_colorTex;
varying lowp vec4 v_color;
#else
varying vec2 v_colorTexCoord;
#endif

varying vec2 v_maskTexCoord;

void main()
{
  // Here we intentionally decrease precision of 'pivot' calculation
  // to eliminate jittering effect in process of billboard reconstruction.
  lowp vec4 pivot = vec4(a_position.xyz, 1.0) * modelView;
  vec4 offset = vec4(a_normal, 0.0, 0.0) * projection;
  
  float pivotZ = a_position.w;
  float zScale = projection[0][0] * length(vec4(1.0, 0.0, 0.0, 0.0) * modelView);
  
  vec4 projectedPivot = pivot * projection;
  float logicZ = projectedPivot.z / projectedPivot.w;
  vec4 transformedPivot = pivotTransform * vec4(projectedPivot.xy, pivotZ * zScale, projectedPivot.w);
  
  vec4 scale = pivotTransform * vec4(1.0, -1.0, 0.0, 1.0);
  gl_Position = vec4(transformedPivot.xy / transformedPivot.w, logicZ, 1.0) + vec4(offset.xy / scale.w * scale.x, 0.0, 0.0);

#ifdef ENABLE_VTF
  v_color = texture2D(u_colorTex, a_colorTexCoord);
#else
  v_colorTexCoord = a_colorTexCoord;
#endif
  v_maskTexCoord = a_maskTexCoord;
}
