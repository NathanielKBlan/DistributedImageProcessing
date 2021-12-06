//#define STB_IMAGE_IMPLEMENTATION
//#include "stb_image.h"

//#define STB_IMAGE_WRITE_IMPLEMENTATION
//#include "stb_image_write.h"

#include <iostream>
#include <stdio.h>
#include <unistd.h>

#include <fstream>
#include <string>
#include <vector>

#include "net.h"
#include "image.h"
#include "kissnet/kissnet.hpp"
namespace kn = kissnet;

//Arguments order: image file name, image out file name, function name (blur, threshold, or upsample), chunks, threads
int main(int argc, char* argv[]){
    // create socket
    int port = 3000;
    // kn::socket<kn::protocol::tcp> server(kn::endpoint("127.0.0.1:3000"));
    // server.bind();
    // server.listen();

    kn::tcp_socket a_socket(kn::endpoint("127.0.0.1:3000"));
    a_socket.connect();

    //Constants go here up top
    int n = 1;
    const int workers = 4; //Assuming 4 workers for now
    // const int hotdogSections = std::stoi(argv[4]);
    const int hotdogSections = 3;

    //Array of loaded images (chopped up)
    //Pop chunk off as it's sent
    std::vector<unsigned char *> chunks;

    //TODO: Split up into chunks and push onto vector
    int w;
    int h;
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

    kn::buffer<4096> static_buffer;

    // send the width of the chunk
    printf("send width %d\n", w);
    a_socket.send(reinterpret_cast<const std::byte *>(&w), sizeof(int));

    const auto [data_size, status_code] = a_socket.recv(static_buffer);

    // To print it as a good old C string, add a null terminator
    if (data_size < static_buffer.size()) {
        static_buffer[data_size] = std::byte{'\0'};
    }

    // Print the raw data as text into the terminal (should display html/css code
    // here)
    std::cout << reinterpret_cast<const char *>(static_buffer.data()) << '\n';

    usleep(1000000);
    // send the height of the chunk
    printf("send chunk_height %d\n", chunk_height);
    a_socket.send(reinterpret_cast<const std::byte *>(&chunk_height), sizeof(int));

    const auto [data_size_1, status_code_1] = a_socket.recv(static_buffer);

    // To print it as a good old C string, add a null terminator
    if (data_size_1 < static_buffer.size()) {
        static_buffer[data_size_1] = std::byte{'\0'};
    }

    // Print the raw data as text into the terminal (should display html/css code
    // here)
    std::cout << reinterpret_cast<const char *>(static_buffer.data()) << '\n';

    // send the data of the chunk 
    std::cout << "send data\n";
    a_socket.send(reinterpret_cast<const std::byte *>(&chunks.at(0)), w * chunk_height);

    // for (int i = 0; i < chunks.size(); i++) {
    //     // TODO: send out chunks to socket
    // }

    //Testing of writing of chunk
    stbi_write_png("../data/chunk1.png", w , chunk_height, n, chunks.at(0), w * n);

    //Array of booleans for assigned chunks
    std::vector<bool> assigned;

    //Array of availability of workers
    std::vector<bool> workerAvailability;

    //Array of image results for chunks;
    std::vector<Image> results;
    
    //Use threads to open connections and send stuff to workers + recieve results
    //Handle specific function reqs like padding for blur and sum for threshold
    //Use mutex for results

    //Aggregate results into an output image
    
}