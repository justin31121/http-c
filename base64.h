//https://stackoverflow.com/questions/342409/how-do-i-base64-encode-decode-in-c
#ifndef BASE64_H_H
#define BASE64_H_H

#include <stdbool.h>
#include <stdint.h>

#ifndef BASE64_DEF
#define BASE64_DEF static inline
#endif //BASE64_DEF

BASE64_DEF bool base64_encode(const char *input, size_t input_size,
		   char *buffer, size_t buffer_size,
		   size_t *output_size);

BASE64_DEF bool base64_decode(const char *input, size_t input_size,
		   char *buffer, size_t buffer_size,
		   size_t *output_size);

#ifdef BASE64_IMPLEMENTATION

static unsigned char base64_decoding_table[] = {
  [43] = 62, [47] = 63, [48] = 52,
  [49] = 53, [50] = 54, [51] = 55,
  [52] = 56, [53] = 57, [54] = 58,
  [55] = 59, [56] = 60, [57] = 61,
  [65] =  0, [66] =  1, [67] =  2,
  [68] =  3, [69] =  4, [70] =  5,
  [71] =  6, [72] =  7, [73] =  8,
  [74] =  9, [75] = 10, [76] = 11,
  [77] = 12, [78] = 13, [79] = 14,
  [80] = 15, [81] = 16, [82] = 17,
  [83] = 18, [84] = 19, [85] = 20,
  [86] = 21, [87] = 22, [88] = 23,
  [89] = 24, [90] = 25, [97] = 26,
  [98] = 27, [99] = 28, [100] = 29,
  [101] = 30, [102] = 31, [103] = 32,
  [104] = 33, [105] = 34, [106] = 35,
  [107] = 36, [108] = 37, [109] = 38,
  [110] = 39, [111] = 40, [112] = 41,
  [113] = 42, [114] = 43, [115] = 44,
  [116] = 45, [117] = 46, [118] = 47,
  [119] = 48, [120] = 49, [121] = 50,
  [122] = 51
};

BASE64_DEF bool base64_decode(const char *input, size_t input_size,
		   char *buffer, size_t buffer_size,
		   size_t *output_size) {

  if(!input) {
    return false;
  }

  if(!buffer) {
    return false;
  }

  if(!output_size) {
    return false;
  }
  
  if (input_size % 4 != 0) {
    return false;
  }

  size_t output_len = input_size / 4 * 3;
  if (input[input_size - 1] == '=') {
    output_len--;
  }
  if (input[input_size - 2] == '=') {
    output_len--;
  }

  if(output_len > buffer_size) {
    return false;
  }

  for (size_t i = 0, j = 0; i < input_size;) {
    uint32_t sextet_a = input[i] == '=' ? 0 & i++ : base64_decoding_table[(int) input[i++]];
    uint32_t sextet_b = input[i] == '=' ? 0 & i++ : base64_decoding_table[(int) input[i++]];
    uint32_t sextet_c = input[i] == '=' ? 0 & i++ : base64_decoding_table[(int) input[i++]];
    uint32_t sextet_d = input[i] == '=' ? 0 & i++ : base64_decoding_table[(int) input[i++]];

    uint32_t triple = (sextet_a << 3 * 6)
      + (sextet_b << 2 * 6)
      + (sextet_c << 1 * 6)
      + (sextet_d << 0 * 6);

    if (j < output_len) buffer[j++] = (triple >> 2 * 8) & 0xFF;
    if (j < output_len) buffer[j++] = (triple >> 1 * 8) & 0xFF;
    if (j < output_len) buffer[j++] = (triple >> 0 * 8) & 0xFF;

  }
  
  *output_size = output_len;

  return true;
}

static unsigned char base64_encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
  'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
  'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
  'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
  'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
  'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
  'w', 'x', 'y', 'z', '0', '1', '2', '3',
  '4', '5', '6', '7', '8', '9', '+', '/'};
static int mod_table[] = {0, 2, 1};

BASE64_DEF bool base64_encode(const char *input, size_t input_size,
		   char *buffer, size_t buffer_size,
		   size_t *output_size) {

  size_t output_len = 4 * ((input_size + 2) / 3);
  if(!input) {
    return false;
  }

  if(!buffer) {
    return false;
  }
  
  if(!output_size) {
    return false;
  }

  if(output_len > buffer_size) {
    return false;
  }

  *output_size = output_len;

  for (size_t i=0,j=0;i<input_size;) {
    uint32_t octet_a = i < input_size ? (unsigned char)input[i++] : 0;
    uint32_t octet_b = i < input_size ? (unsigned char)input[i++] : 0;
    uint32_t octet_c = i < input_size ? (unsigned char)input[i++] : 0;

    uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

    buffer[j++] = base64_encoding_table[(triple >> 3 * 6) & 0x3F];
    buffer[j++] = base64_encoding_table[(triple >> 2 * 6) & 0x3F];
    buffer[j++] = base64_encoding_table[(triple >> 1 * 6) & 0x3F];
    buffer[j++] = base64_encoding_table[(triple >> 0 * 6) & 0x3F];
  }

  for (int i = 0; i < mod_table[input_size % 3]; i++) {
    buffer[output_len - 1 - i] = '=';
  }
  
  return true;
}

#endif //BASE64_IMPLEMENTATION

#endif //BASE64_H_H
