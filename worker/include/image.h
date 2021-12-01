#ifndef _IMAGE_H
#define _IMAGE_H

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

class Image {
 public:
  Image(const char* path) {
    data = stbi_load(path, &w, &h, &n, 1);
    n = 1;
  }
  Image(int w, int h) {
    this->w = w;
    this->h = h;
    n = 1;
    data = new unsigned char[w * h];
  }
  ~Image() { stbi_image_free(data); }

  int get(int x, int y) { return data[y * w + x]; }
  void set(int x, int y, int c) { data[y * w + x] = c; }
  void write(const char* path) { stbi_write_png(path, w, h, n, data, w * n); }

  unsigned char* data;
  int w;
  int h;
  int n;
};

#endif /* _IMAGE_H */