#version 100

attribute vec2 position;
attribute vec4 color;
attribute vec2 uv;

varying vec4 out_color;
varying vec2 out_uv;

void main() {
  gl_Position = vec4(position, 0, 1);
  out_color = color;
  out_uv = uv;
}