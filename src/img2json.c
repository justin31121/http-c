#include <stdio.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../thirdparty/stb_image.h"

int main(int argc, char **argv) {

    if(argc < 3) {
	fprintf(stderr, "ERROR: Please provide enough arguments\n");
	fprintf(stderr, "USAGE: %s <img-in> <json-out>\n", argv[0]);
	return 1;
    }

    const char *input = argv[1];
    const char *output = argv[2];

    int width, height;
    unsigned char *data = stbi_load(input, &width, &height, 0, 4);
    if(!data) {
	fprintf(stderr, "ERROR: Can not load image: '%s'\n", input);
	return 1;
    }
    
    FILE *f = fopen(output, "wb");
    if(!f) {
	fprintf(stderr, "ERROR: Can not open output: '%s'\n", output);
	return 1;
    }

    size_t input_len = strlen(input);
    int k = (int) input_len;
    while(k>1 && input[k-1] != '\\') k--;

    fprintf(f, "{\n");
    fprintf(f, "\t\"width\": %d,\n", width);
    fprintf(f, "\t\"height\": %d,\n", height);
    fprintf(f, "\t\"filename\": \"%s\",\n", input + k);
    fprintf(f, "\t\"data\": [", height);

    int n = width*height*4;
    for(int i=0;i<n;i++) {
	unsigned char byte = data[i];
	fprintf(f, "%u", byte);
	if(i != n - 1) fprintf(f, ", ");
    }
    
    fprintf(f, "\t]\n");
    fprintf(f, "}");
    
    
    return 0;		 
}
