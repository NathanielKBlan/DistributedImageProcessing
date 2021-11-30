#include <stdio.h>
#include <unistd.h>

#include <fstream>
#include <string>
#include <vector>

#include "net.h"
#include "image.h"

//Arguments order: image file name, image out file name, function name (blur, threshold, or upsample), chunks, threads
int main(int argc, char* argv[]){

    //Array of loaded images (chopped up)
    //Pop chunk off as it's sent
    std::vector<Image> chunks;

    //TODO: Split up into chunks and push onto vector

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