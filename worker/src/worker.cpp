#include <iostream>
#include <omp.h>
#include <chrono>
#include <thread>

#include "image.h"
#include "kissnet/kissnet.hpp"
namespace kn = kissnet;

int main() {
  kn::socket<kn::protocol::tcp> server(kn::endpoint("127.0.0.1:3000"));
  server.bind();
  server.listen();

  while (true) {
    auto client = server.accept();
    printf("socket accept\n");

    kn::buffer<1024> buff;
    const auto [size, status] = client.recv(buff);


    std::cout << "size: " << size << std::endl;
    std::cout << "status: " << status << std::endl;
    auto buff_data = reinterpret_cast<const int *>(buff.data());
    std::cout << "received: " << *buff_data << std::endl;

    std::string response = "Success";
    client.send(reinterpret_cast<const std::byte *>(response.c_str()), response.size());
  }

  return 0;
}

Image upsample(Image img, int scale, int threads){
  
  Image out_img(img.w * scale, img.h * scale);

  omp_set_num_threads(threads);

  #pragma omp parallel for
  for(int x = 0; x < img.w; x++){ //This set of nested loops is used to iterate over each pixel and fetch its value
    for(int y = 0; y < img.h; y++){

      int pixelValue = img.get(x, y);

      //This set of nested loops takes care of filling up the scaled region w/ the same pixel value
      for(int a = 0; a < scale; a++){
        for(int b = 0; b < scale; b++){
          out_img.set((x * scale) + a, (y * scale) + b, pixelValue);
        }
      }

    }
  }

  return out_img;
}

Image blur(Image img, int padding_amount, int blur_size, int threads){

  omp_set_num_threads(threads);
  
  int width = img.w - padding_amount;

  Image out_img(width, img.h);

  #pragma omp parallel for
  for(int x = 0; x < width; x++){
    for(int y = 0; y < img.h; y++){

      if(blur_size >= 1){

        int values[blur_size][blur_size];
        values[(blur_size / 2) + 1][(blur_size / 2) + 1] = img.get(x, y);

        for(int a = 0; a < blur_size; a++){

          for(int b = 0; b < blur_size; b++){

            if(x - (blur_size / 2) + a >= 0 && x - (blur_size / 2) + a < img.w && y - (blur_size / 2) + b >= 0 && y - (blur_size / 2) + b < img.h){
              values[a][b] = img.get(x - (blur_size / 2) + a, y - (blur_size / 2) + b);
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

        out_img.set(x, y, average);
      }else{
        out_img.set(x, y, img.get(x, y));
      }
      
    }
  }

  return out_img;
}

Image threshold(Image img, int threads){
             
  Image out_img(img.w, img.h); // Creates an output image (img.w x img.h)

  // TODO: find the threshold (average of the image using OpenMP)
  double outerConstant = 1.0 / (img.w * img.h);
  int sum = 0;

  omp_set_num_threads(threads);

  #pragma omp parallel for reduction(+:sum)
  for(int x = 0; x < img.w; x++){
      for(int y = 0; y < img.h; y++){
        sum += img.get(x,y);
      }
  }

  //TODO: Send sum back to server
  //TODO: Receive aggregate sum from server

  int threshold = (int) std::round(outerConstant * sum);

  // TODO: threshold the image (using OpenMP)
  #pragma omp parallel for
  for(int x = 0; x < img.w; x++){
    for(int y = 0; y < img.h; y++){
      int currentValue = img.get(x, y);

      if(currentValue < threshold){
        out_img.set(x, y, 0);
      }else{
        out_img.set(x, y, 255);
      }
    }
  }

  return out_img;
}