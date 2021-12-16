//#define STB_IMAGE_IMPLEMENTATION
//#include "stb_image.h"

//#define STB_IMAGE_WRITE_IMPLEMENTATION
//#include "stb_image_write.h"

#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <omp.h>

#include <fstream>
#include <string>
#include <vector>

#include "net.h"
#include "image.h"
#include "kissnet/kissnet.hpp"
namespace kn = kissnet;

#define METADATA_MAX_LENGTH 44
#define IMG_DATA_BUFF_SIZE 4096

void msg_recv(std::vector<kn::tcp_socket> &workers, int i, std::vector<bool> &assigned, unsigned char *chunk, std::vector<unsigned char *> &chunks) {
    bool image_received = false;
    bool metadata_received = false;

    kn::buffer<METADATA_MAX_LENGTH> metadata_buff;

    // printf("waiting for worker %d\n", i);
    auto [size, status] = workers.at(i).recv(metadata_buff);

    while (size <= 0) {
        auto [nsize, nstatus] = workers.at(i).recv(metadata_buff);
        size = nsize;
    }

    std::cout << "metadata received: " << size << std::endl;
    const char * metadata_buff_data = reinterpret_cast<const char *>(metadata_buff.data());

    std::string img_metadata(metadata_buff_data);

    std::cout << "metadata string length: " << img_metadata.length() << std::endl;

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
    int width = std::stoi(widthS);
    int height = std::stoi(heightS);
    int op = std::stoi(opS);
    int id = std::stoi(idS);

    std::cout << "Here is the height: " << height << std::endl;
    std::cout << "Here is the width: " << width << std::endl;
    std::cout << "Here is the op: " << op << std::endl;
    std::cout << "Here is the id: " << id << std::endl;

    // for checking if received full image (actual == calculated)
    size_t actual_size = 0;
    size_t calculated_size = width * height * sizeof(unsigned char);
    unsigned char *received_img = new unsigned char[width * height];

    kn::buffer<IMG_DATA_BUFF_SIZE> img_buff;

    while (actual_size < calculated_size) {
        auto [nsize, nstatus] = workers.at(i).recv(img_buff);
        std::cout << "size received: " << nsize << std::endl;
        std::cout << "status: " << nstatus << std::endl;

        auto buff_data = reinterpret_cast<unsigned char *>(img_buff.data());

        for(int i = 0; i < nsize; i++){
          received_img[i + actual_size] = buff_data[i];
        }

        actual_size += nsize;

        printf("total vs len %u vs %u\n", actual_size, calculated_size);
    }

    std::string filepath = "../data/received-chunk-";
    filepath += std::to_string(id);
    filepath += ".png";

    stbi_write_png(filepath.c_str(), width, height, 1, received_img, width * 1);
    free(received_img);

    #pragma omp critical
    {
        assigned.at(i) = false;
    }
    
}

void metadata_padding(std::string &metadata) {
    size_t lenx = METADATA_MAX_LENGTH - metadata.length() - 1;
    for (int i = 0; i < lenx; i++) {
        metadata += "x";
    }
}

//Arguments order: image file name, image out file name, function name (blur, threshold, or upsample), chunks, threads, blur_size, upscale_size
int main(int argc, char* argv[]){

    //Array of booleans of availability of workers
    std::vector<bool> assigned = {false, false, false, false};

    //Array of image results for chunks;
    std::vector<Image> results;

    //Vector of sockets
    std::vector<kn::tcp_socket> workers; 
    workers.push_back(kn::endpoint("127.0.0.1:3000"));
    workers.push_back(kn::endpoint("127.0.0.1:3001"));
    workers.push_back(kn::endpoint("127.0.0.1:3002"));
    workers.push_back(kn::endpoint("127.0.0.1:3003"));


    //Vector of clients
    std::vector<kn::tcp_socket> clients;

    for(int i = 0; i < workers.size(); i++){
        workers.at(i).connect();
    }

    //Constants go here up top
    int n = 1;
    const int hotdogSections = std::stoi(argv[4]);

    //Array of loaded images (chopped up)
    //Pop chunk off as it's sent
    std::vector<unsigned char *> chunks;

    //TODO: Split up into chunks and push onto vector
    int w;
    int h;
    int op = std::stoi(argv[3]);
    int blur_size = 0;
    int upscale_size = 0;
    int sum = 0;

    if(op == 2){
        blur_size = std::stoi(argv[6]);
    }else if(op == 3){
        upscale_size = std::stoi(argv[6]);
    }

    unsigned char * data = stbi_load(argv[1], &w, &h, &n, 1);
    n = 1;  // so that stbi_write_png works for greyscale

    int chunk_height = h / hotdogSections;
    for(int chunk = 0; chunk < hotdogSections; chunk++){

        unsigned char *hotdogData = new unsigned char[w * chunk_height];

        for(int x = 0; x < w; x++){     // looping through each column

            for(int y = 0; y < chunk_height; y++){      // looping through each row of a chunk 
                
                hotdogData[y * w + x] = data[(y+chunk * chunk_height) * w + x];
                sum += hotdogData[y * w + x];

            }
        }

        chunks.push_back(hotdogData);

    }

    unsigned char * lastChunk = chunks.at(chunks.size() - 1);

    while(chunks.size() != 0){

        //Send work to any available worker
        #pragma omp parallel for 
        for(int i = 0; i < assigned.size(); i++){
            if(assigned.at(i) == false && chunks.size() != 0){

                std::cout << "chunks left " << chunks.size() << std::endl;

                int chunk_id;
                unsigned char *chunk;

                #pragma omp critical
                {
                    chunk_id = chunks.size() - 1;
                    printf("thread id %d chunk id %d\n", omp_get_thread_num(), chunk_id);
                    chunk = chunks.back();
                    chunks.pop_back();
                    assigned.at(i) = true;
                }

                std::string img_metadata = std::to_string(w) + "," + std::to_string(chunk_height) + "," + std::to_string(op) + "," + std::to_string(chunk_id) + ",";
                
                if(op == 1){

                     //combine width, height, and op code in a string
                    img_metadata += std::to_string(sum) + "," + std::to_string(w) + "," + std::to_string(h);
                    metadata_padding(img_metadata);

                    // std::cout << "Sending image metadata: " << img_metadata << std::endl;
                    workers.at(i).send(reinterpret_cast<const std::byte *>(img_metadata.c_str()), sizeof(unsigned char) * (img_metadata.length() + 1));

                     // send the data of the chunk 
                    // std::cout << "send data\n";
                    const auto [send_size, send_status] = workers.at(i).send(reinterpret_cast<const std::byte *>(chunk), w * chunk_height * sizeof(unsigned char));

                }else if(op == 2){
                    //combine width, height, and op code in a string
                    img_metadata += std::to_string(blur_size) + "," + std::to_string(hotdogSections) + ",";
                    metadata_padding(img_metadata);

                    // std::cout << "Sending image metadata: " << img_metadata << std::endl;
                    workers.at(i).send(reinterpret_cast<const std::byte *>(img_metadata.c_str()), sizeof(unsigned char) * (img_metadata.length() + 1));

                     // send the data of the chunk 
                    // std::cout << "send data\n";
                    if(chunk_id == 0){

                        unsigned char * paddedChunk = new unsigned char[(w * chunk_height) + (w * (blur_size / 2))];

                        for(int x = 0; x < w; x++){     // looping through each column

                            for(int y = 0; y < chunk_height + (blur_size / 2); y++){      // looping through each row of a chunk 
                
                                if(y < chunk_height){

                                    paddedChunk[y * w + x] = chunk[y * w + x];

                                }else{

                                    paddedChunk[y * w + x] = lastChunk[(y - chunk_height) * w + x];
                                }

                            }
                        }

                        const auto [send_size, send_status] = workers.at(i).send(reinterpret_cast<const std::byte *>(paddedChunk), (w * chunk_height) + (w * (blur_size / 2)) * sizeof(unsigned char));
                        std::cout << "sent: " << send_size << ", " << (w * chunk_height) + (w * (blur_size / 2)) <<  std::endl;

                    }else if(chunk_id == hotdogSections - 1){

                        unsigned char * paddedChunk = new unsigned char[(w * chunk_height) + (w * (blur_size / 2))];

                        for(int x = 0; x < w; x++){     // looping through each column

                            for(int y = 0; y < chunk_height + (blur_size / 2); y++){      // looping through each row of a chunk 
                
                                if(y < (blur_size / 2)){

                                    paddedChunk[y * w + x] = chunks.back()[y * w + x];

                                }else{

                                    paddedChunk[y * w + x] = chunk[(y - (blur_size / 2)) * w + x];

                                }

                            }
                        }
                        
                        const auto [send_size, send_status] = workers.at(i).send(reinterpret_cast<const std::byte *>(paddedChunk), (w * chunk_height) + (w * (blur_size / 2)) * sizeof(unsigned char));
                        std::cout << "sent: " << send_size << ", " << (w * chunk_height) + (w * (blur_size / 2)) <<  std::endl;

                    }else{

                        unsigned char * paddedChunk = new unsigned char[(w * chunk_height) + 2 * (w * (blur_size / 2))];

                        for(int x = 0; x < w; x++){     // looping through each column

                            for(int y = 0; y < chunk_height + 2 * (blur_size / 2); y++){      // looping through each row of a chunk 
                
                                if(y < (blur_size / 2)){

                                    paddedChunk[y * w + x] = chunks.back()[y * w + x];

                                }else if(y < chunk_height + (blur_size / 2)){

                                    paddedChunk[y * w + x] = chunk[(y - (blur_size / 2)) * w + x];

                                }else{

                                    paddedChunk[y * w + x] = lastChunk[(y - (chunk_height + (blur_size / 2))) * w + x];
                                    lastChunk = chunk;

                                }

                            }
                        }
                        
                        const auto [send_size, send_status] = workers.at(i).send(reinterpret_cast<const std::byte *>(paddedChunk), (w * chunk_height) + 2 *  (w * (blur_size / 2)) * sizeof(unsigned char));
                        std::cout << "sent: " << send_size << ", " << (w * chunk_height) + (w * 2 * (blur_size / 2)) <<  std::endl;

                    }

                }else{
                    //combine width, height, and op code in a string
                    metadata_padding(img_metadata);

                    // std::cout << "Sending image metadata: " << img_metadata << std::endl;
                    workers.at(i).send(reinterpret_cast<const std::byte *>(img_metadata.c_str()), sizeof(unsigned char) * (img_metadata.length() + 1));

                     // send the data of the chunk 
                    // std::cout << "send data\n";
                    const auto [send_size, send_status] = workers.at(i).send(reinterpret_cast<const std::byte *>(chunk), w * chunk_height * sizeof(unsigned char));
                    std::cout << "sent: " << send_size << ", " << w*chunk_height <<  std::endl;
                }

               
                #pragma omp task
                    msg_recv(workers, i, assigned, chunk, chunks);
            }
        }

    }   
    
    //Use threads to open connections and send stuff to workers + recieve results
    //Handle specific function reqs like padding for blur and sum for threshold
    //Use mutex for results

    //Aggregate results into an output image
    
}