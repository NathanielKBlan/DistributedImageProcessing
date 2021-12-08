#include <iostream>
#include <omp.h>
#include <chrono>
#include <thread>
#include <vector>

#include "image.h"
#include "kissnet/kissnet.hpp"
namespace kn = kissnet;

unsigned char * upsample(unsigned char * img, int w, int h, int scale, int threads);
unsigned char * blur(unsigned char * img, int w, int h, int blur_size, int threads);
unsigned char * threshold(unsigned char * img, int w, int h, int threads);

int main() {

  int width = 0;
  int height = 0;
  int op = 0;
  int n = 1;
  unsigned char * imageChunk;

  kn::socket<kn::protocol::tcp> server(kn::endpoint("127.0.0.1:3000"));
  server.bind();
  server.listen();

  auto client = server.accept();
  printf("socket accept\n");

  bool detectedImage = false;
  int lastIndex = 0;

  while (true) {

    if(width == 0 || height == 0 || op == 0){
      kn::buffer<1024> buff;
      const auto [size, status] = client.recv(buff);

      std::cout << "size received: " << size << std::endl;

      if(size != 0){
        const char * buff_data = reinterpret_cast<const char *>(buff.data());

        std::string img_metadata(buff_data);

        std::cout << img_metadata << std::endl;

        //split string and assign values
        std::string delim = ",";
        std::string widthS = img_metadata.substr(0, img_metadata.find(delim));
        img_metadata.erase(0, img_metadata.find(delim) + delim.length());
        std::string heightS = img_metadata.substr(0, img_metadata.find(delim));
        img_metadata.erase(0, img_metadata.find(delim) + delim.length());
        std::string opS = img_metadata.substr(0, img_metadata.length());

        //assign values
        width = std::stoi(widthS);
        height = std::stoi(heightS);
        op = std::stoi(opS);

        std::cout << "Here is the height: " << height << std::endl;
        std::cout << "Here is the width: " << width << std::endl;
        std::cout << "Here is the op: " << op << std::endl;

        imageChunk = new unsigned char[width * height];
      }

    }else{

      kn::buffer<2048> buff;
      const auto [size, status] = client.recv(buff);

      
      auto buff_data = reinterpret_cast<unsigned char *>(buff.data());

      if(size > 0){
        
        for(int i = 0; i < size; i++){
          imageChunk[i + lastIndex] = buff_data[i];
          //std::cout << i + lastIndex << std::endl;
        }
        lastIndex += size;
        //std::cout << lastIndex << std::endl;
        detectedImage = true;
      }else if(size == 0 && detectedImage){
        break;
      }

    }
  }

  std::cout << width * height << std::endl;
  stbi_write_png("../data/chunk1-1-2.png", width, height, n, imageChunk, width * n);

  if(op == 1){
    unsigned char * out_img = upsample(imageChunk, width, height, 4, 8);
    stbi_write_png("../data/chunk0-upscaled.png", width * 4, height * 4, n, out_img, (width * 4) * n);
  }else if(op == 2){
    unsigned char * out_img = blur(imageChunk, width, height, 20, 8);
    stbi_write_png("../data/chunk0-blured.png", width, height, n, out_img, width * n);
  }else if(op == 3){
    unsigned char * out_img = threshold(imageChunk, width, height, 8);
    stbi_write_png("../data/chunk0-thresh.png", width, height, n, out_img, width * n);
  }

  return 0;
}

unsigned char * upsample(unsigned char * img, int w, int h, int scale, int threads){

  std::cout << "Upsampling" << std::endl;
  
  unsigned char * out_img = new unsigned char[w * h * scale * scale];

  omp_set_num_threads(threads);

  #pragma omp parallel for
  for(int x = 0; x < w; x++){ //This set of nested loops is used to iterate over each pixel and fetch its value
    for(int y = 0; y < h; y++){

      int pixelValue = img[y * w + x];

      //This set of nested loops takes care of filling up the scaled region w/ the same pixel value
      for(int a = 0; a < scale; a++){
        for(int b = 0; b < scale; b++){
          //x = (x * scale) + a
          //y = (y * scale) + b
          //std::cout << x << " " << y << std::endl;
          out_img[((y * scale) + b) * (w * scale) + ((x * scale) + a)] = pixelValue;
        }
      }

    }
  }

  return out_img;
}

unsigned char * blur(unsigned char * img, int w, int h, int blur_size, int threads){

  omp_set_num_threads(threads);

  unsigned char * out_img = new unsigned char[w * h];

  #pragma omp parallel for
  for(int x = 0; x < w; x++){
    for(int y = 0; y < h; y++){

      if(blur_size >= 1){

        int values[blur_size][blur_size];
        values[(blur_size / 2) + 1][(blur_size / 2) + 1] = img[y * w + x];

        for(int a = 0; a < blur_size; a++){

          for(int b = 0; b < blur_size; b++){

            if(x - (blur_size / 2) + a >= 0 && x - (blur_size / 2) + a < w && y - (blur_size / 2) + b >= 0 && y - (blur_size / 2) + b < h){
              //x = x - (blur_size / 2) + a
              //y = y - (blur_size / 2) + b
              values[a][b] = img[(y - (blur_size / 2) + b) * w + (x - (blur_size / 2) + a)];
            }else{
              values[a][b] = 0; 
            } 

          }

        }

        int sum = 0;
        int termNums = 0;

        for(int a = 0; a < blur_size; a++){

          for(int b = 0; b < blur_size; b++){
            sum += values[a][b];
            termNums++;
          }

        }

        int average = sum / termNums;

        out_img[y * w + x] = average;
      }else{
        out_img[y * w + x] = img[y * w + x];
      }
      
    }
  }

  return out_img;
}

unsigned char * threshold(unsigned char * img, int w, int h, int threads){
             
  unsigned char * out_img = new unsigned char[w * h]; // Creates an output image (img.w x img.h)

  // TODO: find the threshold (average of the image using OpenMP)
  double outerConstant = 1.0 / (w * h);
  int sum = 0;

  omp_set_num_threads(threads);

  #pragma omp parallel for reduction(+:sum)
  for(int x = 0; x < w; x++){
      for(int y = 0; y < h; y++){
        sum += img[y * w + x];
      }
  }

  //TODO: Send sum back to server
  //TODO: Receive aggregate sum from server

  int threshold = (int) std::round(outerConstant * sum);

  // TODO: threshold the image (using OpenMP)
  #pragma omp parallel for
  for(int x = 0; x < w; x++){
    for(int y = 0; y < h; y++){
      int currentValue = img[y * w + x];

      if(currentValue < threshold){
        out_img[y * w + x] = 0;
      }else{
        out_img[y * w + x] = 255;
      }
    }
  }

  return out_img;
}