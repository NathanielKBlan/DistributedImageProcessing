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

void msg_recv(std::vector<kn::tcp_socket> &workers, int i, std::vector<bool> &assigned) {
    kn::buffer<1024> buff;

    // printf("waiting for worker %d\n", i);
    auto [size, status] = workers.at(i).recv(buff);

    while (size <= 0) {
        auto [nsize, nstatus] = workers.at(i).recv(buff);
        size = nsize;
    }
    std::cout << "size recv: " << size << std::endl;

    #pragma omp critical
    {
        assigned.at(i) = false;
    }
}

//Arguments order: image file name, image out file name, function name (blur, threshold, or upsample), chunks, threads
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

    #pragma omp parallel for
    for(int i = 0; i < 4; i++){
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
    int op = 1;
    unsigned char * data = stbi_load(argv[1], &w, &h, &n, 1);
    n = 1;  // so that stbi_write_png works for greyscale

    int chunk_height = h / hotdogSections;
    for(int chunk = 0; chunk < hotdogSections; chunk++){

        unsigned char *hotdogData = new unsigned char[w * chunk_height];

        for(int x = 0; x < w; x++){     // looping through each column

            for(int y = 0; y < chunk_height; y++){      // looping through each row of a chunk 
                
                hotdogData[y * w + x] = data[(y+chunk * chunk_height) * w + x];

            }
        }

        chunks.push_back(hotdogData);

    }

    while(chunks.size() != 0){

        //Send work to any available worker
        #pragma omp parallel for
        for(int i = 0; i < assigned.size(); i++){
            if(assigned.at(i) == false && chunks.size() != 0){
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
                
                //combine width, height, and op code in a string
                std::string img_metadata = std::to_string(w) + "," + std::to_string(chunk_height) + "," + std::to_string(op) + "," + std::to_string(chunk_id);

                std::cout << "Sending image metadata: " << img_metadata << std::endl;
                workers.at(i).send(reinterpret_cast<const std::byte *>(img_metadata.c_str()), sizeof(unsigned char) * (img_metadata.length() + 1));

                // send the data of the chunk 
                std::cout << "send data\n";
                const auto [send_size, send_status] = workers.at(i).send(reinterpret_cast<const std::byte *>(chunk), w * chunk_height * sizeof(unsigned char));
                            
                    #pragma omp task
                        msg_recv(workers, i, assigned);
            }
        }
    }   
    
    //Use threads to open connections and send stuff to workers + recieve results
    //Handle specific function reqs like padding for blur and sum for threshold
    //Use mutex for results

    //Aggregate results into an output image
    
}