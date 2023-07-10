#ifndef URL_H
#define URL_H

#ifndef URL_DEF
#  define URL_DEF static inline
#endif //URL_DEF

URL_DEF bool url_encode(const char *input, size_t input_size,
			char* output, size_t output_cap,
			size_t *output_size);
URL_DEF bool url_decode(const char *input, size_t input_size,
			char* output, size_t output_cap,
			size_t *output_size);

#ifdef URL_IMPLEMENTATION

URL_DEF bool url_encode(const char *input, size_t input_size,
			char* output, size_t output_cap,
			size_t *output_size) {

  if(!input || !output || !output_size) {
    return false;
  }

  if(input_size >= output_cap) {
    return false;
  }

  const char *hex = "0123456789abcdef";    
  size_t pos = 0;
  for (size_t i = 0; i < input_size; i++) {
    if (('a' <= input[i] && input[i] <= 'z')
	|| ('A' <= input[i] && input[i] <= 'Z')
	|| ('0' <= input[i] && input[i] <= '9')) {
      output[pos++] = input[i];
    } else if(pos + 3 > output_cap) {
      return false;
    } else {
      output[pos++] = '%';
      output[pos++] = hex[(input[i] & 0xf0) >> 4];
      output[pos++] = hex[input[i] & 0xf];
    }
  }

  *output_size = pos;

  return true;  
}

URL_DEF bool url_decode(const char *input, size_t input_size,
			char* output, size_t output_cap,
			size_t *output_size) {
  if(!input || !output || !output_size) {
    return false;
  }

  size_t pos = 0;
  for(size_t i=0;i<input_size;i++) {
    if(pos >= output_cap) {
      return false;
    }
    
    char c = input[i];
    if(c == '%') {
      if(i + 2 >= input_size) {	
	return false;
      }
      char hi = input[i+1];
      if(hi > 57) hi -= 39;
      char lo = input[i+2];
      if(lo > 57) lo -= 39;
      output[pos++] = (hi << 4) | (lo & 0xf);
      i+=2;
    } else if(c == '\\' && i+5 < input_size) {
      bool replace_with_and = true;
      if(input[i+1] != 'u') replace_with_and = false;
      if(input[i+2] != '0') replace_with_and = false;
      if(input[i+3] != '0') replace_with_and = false;
      if(input[i+4] != '2') replace_with_and = false;
      if(input[i+5] != '6') replace_with_and = false;
      if(replace_with_and) {
	output[pos++] = '&';
	i += 5;
      }
    } else {
      output[pos++] = c;
    }
  }

  *output_size = pos;

  return true;
}

#endif //URL_IMPLEMENTATION

#endif //URL_H
