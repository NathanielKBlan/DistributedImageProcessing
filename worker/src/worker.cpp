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

//Arguments: port
int main(int argc, char* argv[]) {

  int width = 0;
  int height = 0;
  int op = 0;
  int id = -1;
  int n = 1;
  unsigned char * imageChunk;

  std::string ip{"127.0.0.1:"};
  std::string end = ip + argv[1];

  kn::socket<kissnet::protocol::tcp> server(kn::endpoint(end.c_str()));
  server.bind();
  server.listen();

  auto client = server.accept();
  printf("socket accept\n");

  bool detectedImage = false;
  int lastIndex = 0;

  size_t total = 0;
  size_t len = 0;

  while (true) {

    if(width == 0 && height == 0 && op == 0 && id == -1){
      kn::buffer<44> buff;
      const auto [size, status] = client.recv(buff);

      if(size != 0){
        // std::cout << "metadata received: " << size << std::endl;
        const char * buff_data = reinterpret_cast<const char *>(buff.data());

        std::string img_metadata(buff_data);

        // std::cout << "metadata string length: " << img_metadata.length() << std::endl;

        std::cout << img_metadata << std::endl;

        //split string and assign values
        std::string delim = ",";
        std::string widthS = img_metadata.substr(0, img_metadata.find(delim));
        img_metadata.erase(0, img_metadata.find(delim) + delim.length());
        std::string heightS = img_metadata.substr(0, img_metadata.find(delim));
        img_metadata.erase(0, img_metadata.find(delim) + delim.length());
        std::string opS = img_metadata.substr(0, img_metadata.find(delim));
        img_metadata.erase(0, img_metadata.find(delim) + delim.length());
        std::string idS = img_metadata.substr(0, img_metadata.length());

        //assign values
        width = std::stoi(widthS);
        height = std::stoi(heightS);
        op = std::stoi(opS);
        id = std::stoi(idS);

        std::cout << "Here is the height: " << height << std::endl;
        std::cout << "Here is the width: " << width << std::endl;
        std::cout << "Here is the op: " << op << std::endl;
        std::cout << "Here is the id: " << id << std::endl;

        imageChunk = new unsigned char[width * height];
        len = width * height * sizeof(unsigned char);
        buff.empty();
      } else {
        // Maybe add a timeout
        // std::cout << "Waiting for master... " << std::endl;
      }

    }else if(detectedImage){
        printf("image detected\n");
          std::string filepath = "../data/chunk-";
          filepath += std::to_string(id);

        //Do the work here
        if(op == 1){
          filepath += "-upscaled.png";
          unsigned char * out_img = upsample(imageChunk, width, height, 4, 8);
          stbi_write_png(filepath.c_str(), width * 4, height * 4, n, out_img, (width * 4) * n);
        }else if(op == 2){
          filepath += "-blurred.png";
          unsigned char * out_img = blur(imageChunk, width, height, 20, 8);
          stbi_write_png(filepath.c_str(), width, height, n, out_img, width * n);
        }else if(op == 3){
          filepath += "-thresh.png";
          unsigned char * out_img = threshold(imageChunk, width, height, 8);
          stbi_write_png(filepath.c_str(), width, height, n, out_img, width * n);
        }

        detectedImage = false;

        //send available message here to master
        auto done_msg = std::string{"Done"};

        printf("Finishes. Send 'done' to client\n");
        client.send(reinterpret_cast<const std::byte *>(done_msg.c_str()), done_msg.size());

        width = 0;
        height = 0;
        op = 0;
        len = 0;
        id = -1;
        total = 0;
        lastIndex = 0;
        free(imageChunk);

    }else{  // Receiving image data
      kn::buffer<2048> buff;
      // printf("total vs len %u vs %u\n", total, len);
      // printf("receiving from client\n");
      const auto [size, status] = client.recv(buff);      
      auto buff_data = reinterpret_cast<unsigned char *>(buff.data());

      if(size > 0){
        std::cout << "size received: " << size << std::endl;
        std::cout << "status: " << status << std::endl;
        for(int i = 0; i < size; i++){
          imageChunk[i + lastIndex] = buff_data[i];
        }

        lastIndex += size;
        total += size;

        printf("total vs len %u vs %u\n", total, len);
        if (total == len) {
          detectedImage = true;
        }
        
      }
    }
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