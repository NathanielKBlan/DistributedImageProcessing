#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <omp.h>
#include <chrono>

#include <fstream>
#include <string>
#include <vector>

#include "net.h"
#include "image.h"
#include "kissnet/kissnet.hpp"
namespace kn = kissnet;

#define METADATA_MAX_LENGTH 55
#define IMG_DATA_BUFF_SIZE 4096

//to run asyncronously, receives "done" messages from the workers and sets their status accordingly
void msg_recv(std::vector<kn::tcp_socket> &workers, int i, std::vector<bool> &assigned, unsigned char *chunk, std::vector<unsigned char *> &chunks, std::vector<unsigned char *> &processed)
{
    //set the buffer for receiving data from worker
    kn::buffer<METADATA_MAX_LENGTH> metadata_buff;

    //receive data from worker
    auto [size, status] = workers.at(i).recv(metadata_buff);

    while (size <= 0)
    {
        auto [nsize, nstatus] = workers.at(i).recv(metadata_buff);
        size = nsize;
    }
    
    const char *metadata_buff_data = reinterpret_cast<const char *>(metadata_buff.data());

    std::string img_metadata(metadata_buff_data);

    //parse through returned metadata on processed image
    std::string delim = ",";
    std::string widthS = img_metadata.substr(0, img_metadata.find(delim));
    img_metadata.erase(0, img_metadata.find(delim) + delim.length());
    std::string heightS = img_metadata.substr(0, img_metadata.find(delim));
    img_metadata.erase(0, img_metadata.find(delim) + delim.length());
    std::string opS = img_metadata.substr(0, img_metadata.find(delim));
    img_metadata.erase(0, img_metadata.find(delim) + delim.length());
    std::string idS = img_metadata.substr(0, img_metadata.length());
    img_metadata.erase(0, img_metadata.find(delim) + delim.length());
    std::string threadsS = img_metadata.substr(0, img_metadata.length());

    //assign values
    int width = std::stoi(widthS);
    int height = std::stoi(heightS);
    int op = std::stoi(opS);
    int id = std::stoi(idS);

    // for checking if received full image (actual == calculated)
    size_t actual_size = 0;
    size_t calculated_size = width * height * sizeof(unsigned char);
    unsigned char *received_img = new unsigned char[width * height];

    //fetch the processed image slice from the worker and store it in vector
    kn::buffer<IMG_DATA_BUFF_SIZE> img_buff;
    while (actual_size < calculated_size)
    {
        auto [nsize, nstatus] = workers.at(i).recv(img_buff);

        auto buff_data = reinterpret_cast<unsigned char *>(img_buff.data());

        for (int i = 0; i < nsize; i++)
        {
            received_img[i + actual_size] = buff_data[i];
        }

        actual_size += nsize;
    }

    processed.at(id) = received_img;

#pragma omp critical
    {
        //change status of worker to available
        assigned.at(i) = false;
    }
}

//used to pad our metadata so that it fits the buffer perfectly on the worker end
void metadata_padding(std::string &metadata)
{
    size_t lenx = METADATA_MAX_LENGTH - metadata.length() - 1;
    for (int i = 0; i < lenx; i++)
    {
        metadata += "x";
    }
}

//Arguments order: image file name, image out file name, function name (blur 2, threshold 1, or upsample 3), chunks, threads, blur_size, upscale_size
int main(int argc, char *argv[])
{

    //Array of booleans of availability of workers
    std::vector<bool> assigned = {false, false, false, false};

    //Vector of sockets
    std::vector<kn::tcp_socket> workers;
    workers.push_back(kn::endpoint("127.0.0.1:3000"));
    workers.push_back(kn::endpoint("127.0.0.1:3001"));
    workers.push_back(kn::endpoint("127.0.0.1:3002"));
    workers.push_back(kn::endpoint("127.0.0.1:3003"));

    //connect to each worker
    for (int i = 0; i < workers.size(); i++)
    {
        workers.at(i).connect();
    }

    //these values never change
    int n = 1;
    
    //the amount of chunks we have
    const int hotdogSections = std::stoi(argv[4]);

    // Array of processed images
    std::vector<unsigned char *> processed(hotdogSections);

    //Array of loaded images (chopped up)
    //Pop chunk off as it's sent
    std::vector<unsigned char *> chunks;

    
    int w;
    int h;
    int op = std::stoi(argv[3]);
    int blur_size = 0;
    int upscale_size = 0;
    int sum = 0;
    int threads = std::stoi(argv[5]);

    //Assign blur_size or upscale_size depending on the op code used
    if (op == 2)
    {
        blur_size = std::stoi(argv[6]);
    }
    else if (op == 3)
    {
        upscale_size = std::stoi(argv[6]);
    }

    //load the specified image
    unsigned char *data = stbi_load(argv[1], &w, &h, &n, 1);
    n = 1; // so that stbi_write_png works for greyscale (n changes after image loading)

    //calculate the height of each chunk
    int chunk_height = h / hotdogSections;
    
    //slice up the image and store each slice on the vector
    for (int chunk = 0; chunk < hotdogSections; chunk++)
    {

        unsigned char *hotdogData = new unsigned char[w * chunk_height];

        for (int x = 0; x < w; x++)
        { // looping through each column

            for (int y = 0; y < chunk_height; y++)
            { // looping through each row of a chunk

                hotdogData[y * w + x] = data[(y + chunk * chunk_height) * w + x];
                sum += hotdogData[y * w + x];
            }
        }

        chunks.push_back(hotdogData);
    }

    //to be used for padding purposes
    unsigned char *lastChunk = chunks.at(chunks.size() - 1);
    unsigned char *firstChunk = chunks.at(0);

    auto start = std::chrono::high_resolution_clock::now();

    //keep sending work while there are still chunks/slices left to process
    while (chunks.size() != 0)
    {

        //Send work to any available worker
        #pragma omp parallel for
        for (int i = 0; i < assigned.size(); i++)
        {
            #pragma omp critical
            {
                //if worker is available we send it a slice to process
                if (assigned.at(i) == false && chunks.size() > 0)
                {
                    int chunk_id = chunks.size() - 1;
                    unsigned char *chunk = chunks.back();
                    chunks.pop_back();
                    
                    //set worker status to busy
                    assigned.at(i) = true;

                    //combine width, height, op code, id, threads in a string
                    std::string img_metadata = std::to_string(w) + "," + std::to_string(chunk_height) + "," + std::to_string(op) + "," + std::to_string(chunk_id) + "," + std::to_string(threads) + ",";

                    if (op == 1)
                    {

                        // add more metadata necessary for thresholding
                        img_metadata += std::to_string(sum) + "," + std::to_string(w) + "," + std::to_string(h);
                        metadata_padding(img_metadata);

                        workers.at(i).send(reinterpret_cast<const std::byte *>(img_metadata.c_str()), sizeof(unsigned char) * (img_metadata.length() + 1));

                        // send the data of the chunk
                        const auto [send_size, send_status] = workers.at(i).send(reinterpret_cast<const std::byte *>(chunk), w * chunk_height * sizeof(unsigned char));
                    }
                    else if (op == 2)
                    {
                        // add more metadata necessary for bluring
                        img_metadata += std::to_string(blur_size) + "," + std::to_string(hotdogSections) + ",";
                        metadata_padding(img_metadata);

                        workers.at(i).send(reinterpret_cast<const std::byte *>(img_metadata.c_str()), sizeof(unsigned char) * (img_metadata.length() + 1));

                        // send the data of the chunk
                        
                        //send chunk with bottom padding only
                        if (chunk_id == 0)
                        {

                            unsigned char *paddedChunk = new unsigned char[(w * chunk_height) + (w * (blur_size / 2))];

                            for (int x = 0; x < w; x++)
                            { // looping through each column

                                for (int y = 0; y < chunk_height + (blur_size / 2); y++)
                                { // looping through each row of a chunk

                                    if (y < chunk_height)
                                    {

                                        paddedChunk[y * w + x] = chunk[y * w + x];
                                    }
                                    else
                                    {

                                        paddedChunk[y * w + x] = lastChunk[(y - chunk_height) * w + x];
                                    }
                                }
                            }

                            const auto [send_size, send_status] = workers.at(i).send(reinterpret_cast<const std::byte *>(paddedChunk), (w * chunk_height) + (w * (blur_size / 2)) * sizeof(unsigned char));
                        }
                        //send chunk with top padding only
                        else if (chunk_id == hotdogSections - 1)
                        {

                            unsigned char *paddedChunk = new unsigned char[(w * chunk_height) + (w * (blur_size / 2))];

                            for (int x = 0; x < w; x++)
                            { // looping through each column

                                for (int y = 0; y < chunk_height + (blur_size / 2); y++)
                                { // looping through each row of a chunk

                                    if (y < (blur_size / 2))
                                    {

                                        paddedChunk[y * w + x] = chunks.back()[y * w + x];
                                    }
                                    else
                                    {

                                        paddedChunk[y * w + x] = chunk[(y - (blur_size / 2)) * w + x];
                                    }
                                }
                            }

                            const auto [send_size, send_status] = workers.at(i).send(reinterpret_cast<const std::byte *>(paddedChunk), (w * chunk_height) + (w * (blur_size / 2)) * sizeof(unsigned char));
                        }
                        //send chunk with both top and bottom padding
                        else
                        {

                            unsigned char *paddedChunk = new unsigned char[(w * chunk_height) + 2 * (w * (blur_size / 2))];

                            for (int x = 0; x < w; x++)
                            { // looping through each column

                                for (int y = 0; y < chunk_height + 2 * (blur_size / 2); y++)
                                { // looping through each row of a chunk

                                    if (y < (blur_size / 2))
                                    {

                                        paddedChunk[y * w + x] = chunks.back()[y * w + x];
                                    }
                                    else if (y < chunk_height + (blur_size / 2))
                                    {

                                        paddedChunk[y * w + x] = chunk[(y - (blur_size / 2)) * w + x];
                                    }
                                    else
                                    {

                                        paddedChunk[y * w + x] = lastChunk[(y - (chunk_height + (blur_size / 2))) * w + x];
                                        lastChunk = chunk;
                                    }
                                }
                            }

                            const auto [send_size, send_status] = workers.at(i).send(reinterpret_cast<const std::byte *>(paddedChunk), (w * chunk_height) + 2 * (w * (blur_size / 2)) * sizeof(unsigned char));
                        }
                    }
                    else
                    {
                        //add more metadata needed for upscale
                        img_metadata += std::to_string(upscale_size) + ",";
                        metadata_padding(img_metadata);

                        workers.at(i).send(reinterpret_cast<const std::byte *>(img_metadata.c_str()), sizeof(unsigned char) * (img_metadata.length() + 1));

                        // send the data of the chunk
                        const auto [send_size, send_status] = workers.at(i).send(reinterpret_cast<const std::byte *>(chunk), w * chunk_height * sizeof(unsigned char));
                    }

                    #pragma omp task
                    msg_recv(workers, i, assigned, chunk, chunks, processed);
                }
            }
        }
    }

    //if image is being upscaled we need to re-adjust our width and chunk height accordingly
    if (op == 3)
    {
        w *= upscale_size;
        chunk_height *= upscale_size;
    }

    
    size_t chunk_len = w * chunk_height;
    int result_h = chunk_height * hotdogSections;
    unsigned char *result = new unsigned char[w * result_h];

    // combine the processed chunks back into 1 result image
    for (int i = 0; i < processed.size(); i++)
    {

        for (int j = 0; j < chunk_len; j++)
        {
            result[i * chunk_len + j] = processed.at(i)[j];
        }
    }

    //write the image to the output destination
    stbi_write_png(argv[2], w, result_h, 1, result, w * 1);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Finishes in: " << duration.count() << " microseconds" << std::endl;
    return 0;
}
