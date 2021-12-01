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

//Arguments order: image file name, image out file name, function name (blur, threshold, or upsample), chunks, threads
int main(int argc, char* argv[]){

    //Constants go here up top
    int n = 1;
    const int workers = 4; //Assuming 4 workers for now
    const int hotdogSections = std::stoi(argv[4]);

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

    //Testing of writing of chunk
    stbi_write_png("../data/chunk1.png", w , chunk_height, n, chunks.at(1), w * n);

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