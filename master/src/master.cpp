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

void msg_recv(std::vector<kn::tcp_socket> &workers, int i, std::vector<bool> &assigned, unsigned char *chunk, std::vector<unsigned char *> &chunks, std::vector<unsigned char *> &processed)
{
    kn::buffer<METADATA_MAX_LENGTH> metadata_buff;

    auto [size, status] = workers.at(i).recv(metadata_buff);

    while (size <= 0)
    {
        auto [nsize, nstatus] = workers.at(i).recv(metadata_buff);
        size = nsize;
    }

    // std::cout << "metadata received: " << size << std::endl;
    const char *metadata_buff_data = reinterpret_cast<const char *>(metadata_buff.data());

    std::string img_metadata(metadata_buff_data);

    //split string and assign values
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

    // std::cout << "Here is the height: " << height << std::endl;
    // std::cout << "Here is the width: " << width << std::endl;
    // std::cout << "Here is the op: " << op << std::endl;
    // std::cout << "Here is the id: " << id << std::endl;

    // for checking if received full image (actual == calculated)
    size_t actual_size = 0;
    size_t calculated_size = width * height * sizeof(unsigned char);
    unsigned char *received_img = new unsigned char[width * height];

    kn::buffer<IMG_DATA_BUFF_SIZE> img_buff;

    while (actual_size < calculated_size)
    {
        auto [nsize, nstatus] = workers.at(i).recv(img_buff);
        // std::cout << "size received: " << nsize << std::endl;

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
        assigned.at(i) = false;
    }
}

void metadata_padding(std::string &metadata)
{
    size_t lenx = METADATA_MAX_LENGTH - metadata.length() - 1;
    for (int i = 0; i < lenx; i++)
    {
        metadata += "x";
    }
}

//Arguments order: image file name, image out file name, function name (blur, threshold, or upsample), chunks, threads, blur_size, upscale_size
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

    for (int i = 0; i < workers.size(); i++)
    {
        workers.at(i).connect();
    }

    //Constants go here up top
    int n = 1;
    const int hotdogSections = std::stoi(argv[4]);

    // Array of processed images
    std::vector<unsigned char *> processed(hotdogSections);

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
    int threads = std::stoi(argv[5]);

    if (op == 2)
    {
        blur_size = std::stoi(argv[6]);
    }
    else if (op == 3)
    {
        upscale_size = std::stoi(argv[6]);
    }

    unsigned char *data = stbi_load(argv[1], &w, &h, &n, 1);
    n = 1; // so that stbi_write_png works for greyscale

    int chunk_height = h / hotdogSections;
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

    unsigned char *lastChunk = chunks.at(chunks.size() - 1);
    unsigned char *firstChunk = chunks.at(0);

    auto start = std::chrono::high_resolution_clock::now();

    while (chunks.size() != 0)
    {

        //Send work to any available worker
        #pragma omp parallel for
        for (int i = 0; i < assigned.size(); i++)
        {
            #pragma omp critical
            {
                if (assigned.at(i) == false && chunks.size() > 0)
                {

                    // std::cout << "chunks left " << chunks.size() << std::endl;

                    int chunk_id = chunks.size() - 1;
                    // printf("thread id %d chunk id %d\n", omp_get_thread_num(), chunk_id);
                    unsigned char *chunk = chunks.back();
                    chunks.pop_back();
                    assigned.at(i) = true;

                    //combine width, height, op code, id, threads in a string
                    std::string img_metadata = std::to_string(w) + "," + std::to_string(chunk_height) + "," + std::to_string(op) + "," + std::to_string(chunk_id) + "," + std::to_string(threads) + ",";

                    if (op == 1)
                    {

                        // add more metadata
                        img_metadata += std::to_string(sum) + "," + std::to_string(w) + "," + std::to_string(h);
                        metadata_padding(img_metadata);

                        workers.at(i).send(reinterpret_cast<const std::byte *>(img_metadata.c_str()), sizeof(unsigned char) * (img_metadata.length() + 1));

                        // send the data of the chunk
                        const auto [send_size, send_status] = workers.at(i).send(reinterpret_cast<const std::byte *>(chunk), w * chunk_height * sizeof(unsigned char));
                    }
                    else if (op == 2)
                    {
                        //combine width, height, and op code in a string
                        img_metadata += std::to_string(blur_size) + "," + std::to_string(hotdogSections) + ",";
                        metadata_padding(img_metadata);

                        // std::cout << "Sending image metadata: " << img_metadata << std::endl;
                        workers.at(i).send(reinterpret_cast<const std::byte *>(img_metadata.c_str()), sizeof(unsigned char) * (img_metadata.length() + 1));

                        // send the data of the chunk
                        // std::cout << "send data\n";
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
                            // std::cout << "sent: " << send_size << ", " << (w * chunk_height) + (w * (blur_size / 2)) << std::endl;
                        }
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
                            // std::cout << "sent: " << send_size << ", " << (w * chunk_height) + (w * (blur_size / 2)) << std::endl;
                        }
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
                            // std::cout << "sent: " << send_size << ", " << (w * chunk_height) + (w * 2 * (blur_size / 2)) <<  std::endl;
                        }
                    }
                    else
                    {
                        img_metadata += std::to_string(upscale_size) + ",";
                        metadata_padding(img_metadata);

                        // std::cout << "Sending image metadata: " << img_metadata << std::endl;
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

    stbi_write_png(argv[2], w, result_h, 1, result, w * 1);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Finishes in: " << duration.count() << " microseconds" << std::endl;
    return 0;
}