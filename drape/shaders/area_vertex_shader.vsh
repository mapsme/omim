attribute vec3 a_position;
attribute vec2 a_colorTexCoords;
attribute vec2 a_noisePosition;

uniform mat4 modelView;
uniform mat4 projection;

varying vec2 v_colorTexCoords;
varying vec2 v_position;

void main(void)
{
  v_colorTexCoords = a_colorTexCoords;
  v_position = a_noisePosition;
  gl_Position = vec4(a_position, 1) * modelView * projection;
}
