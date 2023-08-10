#version 100

precision mediump float;

varying vec4 out_color;
varying vec2 out_uv;

void main() {
  gl_FragColor = out_color;
}