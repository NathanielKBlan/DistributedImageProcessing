#include <iostream>
#include <omp.h>
#include <chrono>
#include <thread>
#include <vector>

#include "image.h"
#include "kissnet/kissnet.hpp"
namespace kn = kissnet;

#define METADATA_MAX_LENGTH 55
#define IMG_DATA_BUFF_SIZE 4096

unsigned char *upsample(unsigned char *img, int w, int h, int scale, int threads);
unsigned char *blur(unsigned char *img, int w, int h, int blur_size, int threads);
unsigned char *threshold(unsigned char *img, int w, int h, int sum, double outerConstant, int threads);

static int starting_y = 0;

//Arguments: port
int main(int argc, char *argv[])
{

	//Metadata for image chunk
	int width = 0;
	int height = 0;
	int op = 0;
	int id = -1;
	
	//Default n value, never changes
	int n = 1;
	
	//amount of threads to be used by OpenMP
	int threads = 0;
	
	//blur box size, only gets a value if the op code is 2
	int blur_size;
	
	//upscale amount, only gets a value if the op code is 3
	int upscale;
	
	//pointer to store the chunk
	unsigned char *imageChunk;
	
	//sum for threshold, only gets value other than 0 if op code is 1
	int sum = 0;
	
	//outerConstant for threshold, only gets value other than 0 if op code is 1
	double outerConstant = 0;

	//Start connection to master node on designated port
	std::string ip{"127.0.0.1:"};
	std::string end = ip + argv[1];

	kn::socket<kissnet::protocol::tcp> server(kn::endpoint(end.c_str()));
	server.bind();
	server.listen();

	//Accept incoming connection from master node
	auto client = server.accept();

	//flag for image fecthing
	bool detectedImage = false;
	
	//to be used to fetch image and to verify that whole image has been fetched
	size_t lastIndex = 0;
	size_t len = 0;

	while (true)
	{
		
		//If meta data is at default that means we still have to fetch it
		if (width == 0 && height == 0 && op == 0 && id == -1 && threads == 0)
		{
			//set the buffer for fetching metadata
			kn::buffer<METADATA_MAX_LENGTH> buff;
			
			//receive incoming data
			const auto [size, status] = client.recv(buff);

			//if data was received start parsing the metadata
			if (size != 0)
			{
				const char *buff_data = reinterpret_cast<const char *>(buff.data());

				std::string img_metadata(buff_data);

				//parse the metadata
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

				//assign metadata values
				width = std::stoi(widthS);
				height = std::stoi(heightS);
				op = std::stoi(opS);
				id = std::stoi(idS);
				threads = std::stoi(threadsS);

				//Here we fetch further metadata depending on the op code
				if (op == 1)
				{
					//parse additional metadata needed for threshold
					img_metadata.erase(0, img_metadata.find(delim) + delim.length());
					std::string sumS = img_metadata.substr(0, img_metadata.length());
					img_metadata.erase(0, img_metadata.find(delim) + delim.length());
					std::string totalWS = img_metadata.substr(0, img_metadata.length());
					img_metadata.erase(0, img_metadata.find(delim) + delim.length());
					std::string totalHS = img_metadata.substr(0, img_metadata.length());
					int totalW = std::stoi(totalWS);
					int totalH = std::stoi(totalHS);
					sum = std::stoi(sumS);
					outerConstant = 1.0 / (totalH * totalW);

					//allocate memory for image chunk and set the len variable
					imageChunk = new unsigned char[width * height];
					len = width * height * sizeof(unsigned char);
					buff.empty();
				}
				else if (op == 2)
				{
					//parse additional metadata needed for blur
					img_metadata.erase(0, img_metadata.find(delim) + delim.length());
					std::string blurSizeS = img_metadata.substr(0, img_metadata.length());
					img_metadata.erase(0, img_metadata.find(delim) + delim.length());
					std::string totalSections = img_metadata.substr(0, img_metadata.length());
					blur_size = std::stoi(blurSizeS);
					int lastID = std::stoi(totalSections) - 1;

					//To handle edge cases must allocate memory for the padding
					if (id == 0 || id == lastID)
					{
						//allocate memory for the top or bottom padding and set len
						imageChunk = new unsigned char[(width * height) + (width * (blur_size / 2))];
						len = (width * height) + (width * (blur_size / 2)) * sizeof(unsigned char);
						buff.empty();
						
						//if we are working on the last chunk of the image set the starting_y to the start of the padding
						if (id == lastID)
						{
							starting_y = (blur_size / 2);
						}
					}
					else
					{
						//allocate memory for the top and bottom padding and set len
						imageChunk = new unsigned char[(width * height) + 2 * (width * (blur_size / 2))];
						len = (width * height) + 2 * (width * (blur_size / 2)) * sizeof(unsigned char);
						buff.empty();

						//set the starting_y to the start of the top padding
						starting_y = (blur_size / 2);
					}
				}
				else
				{
					//parse additional metadata needed for upscale
					img_metadata.erase(0, img_metadata.find(delim) + delim.length());
					std::string upscaleS = img_metadata.substr(0, img_metadata.length());
					upscale = std::stoi(upscaleS);

					//allocate memory for image chunk and set the len variable
					imageChunk = new unsigned char[width * height];
					len = width * height * sizeof(unsigned char);
					buff.empty();
				}
			}
		}
		
		//once image has been fully fetched process the image
		else if (detectedImage)
		{
			//to store processed image
			unsigned char *processed_img;

			//upsample
			if (op == 3)
			{
				processed_img = upsample(imageChunk, width, height, upscale, threads);
				width *= upscale;
				height *= upscale;
			}
			//blur
			else if (op == 2)
			{
				processed_img = blur(imageChunk, width, height, blur_size, threads);
			}
			//threshold
			else if (op == 1)
			{
				processed_img = threshold(imageChunk, width, height, sum, outerConstant, threads);
			}

			//set flag to false to signal processing is done
			detectedImage = false;

			//send available message here to master
			std::string img_metadata = std::to_string(width) + "," + std::to_string(height) + "," + std::to_string(op) + "," + std::to_string(id) + "," + std::to_string(threads) + ",";
			size_t lenx = METADATA_MAX_LENGTH - img_metadata.length() - 1;
			for (int i = 0; i < lenx; i++)
			{
				img_metadata += "x";
			}

			//send the processed chunk back to the master
			client.send(reinterpret_cast<const std::byte *>(img_metadata.c_str()), img_metadata.length() + 1);
			client.send(reinterpret_cast<const std::byte *>(processed_img), width * height * sizeof(unsigned char));

			//reset all variables back to their default
			width = 0;
			height = 0;
			op = 0;
			len = 0;
			id = -1;
			lastIndex = 0;
			threads = 0;
			free(imageChunk);
		}
		// Receiving image data
		else
		{ 
			//set our buffer and start receiving the image data
			kn::buffer<IMG_DATA_BUFF_SIZE> buff;
			const auto [size, status] = client.recv(buff);
			auto buff_data = reinterpret_cast<unsigned char *>(buff.data());

			if (size > 0)
			{
				for (int i = 0; i < size; i++)
				{
					//imageChunk is written to from where it last left off
					imageChunk[i + lastIndex] = buff_data[i];
				}

				//update last index to include the size that has been read from the buffer
				lastIndex += size;

				//if we read everything there is to read for the image set the detectedImage flag to true
				if (lastIndex == len)
				{
					detectedImage = true;
				}
			}
		}
	}

	return 0;
}

unsigned char *upsample(unsigned char *img, int w, int h, int scale, int threads)
{	
	unsigned char *out_img = new unsigned char[w * h * scale * scale];

	omp_set_num_threads(threads);

#pragma omp parallel for
	for (int x = 0; x < w; x++)
	{ 
		for (int y = starting_y; y < h; y++)
		{

			int pixelValue = img[y * w + x];

			for (int a = 0; a < scale; a++)
			{
				for (int b = 0; b < scale; b++)
				{
					out_img[((y * scale) + b) * (w * scale) + ((x * scale) + a)] = pixelValue;
				}
			}
		}
	}

	return out_img;
}

unsigned char *blur(unsigned char *img, int w, int h, int blur_size, int threads)
{

	omp_set_num_threads(threads);

	unsigned char *out_img = new unsigned char[w * h];

#pragma omp parallel for
	for (int x = 0; x < w; x++)
	{
		for (int y = starting_y; y < h; y++)
		{
			if (blur_size >= 1)
			{
				int values[blur_size][blur_size];
				values[(blur_size / 2) + 1][(blur_size / 2) + 1] = img[y * w + x];

				for (int a = 0; a < blur_size; a++)
				{
					for (int b = 0; b < blur_size; b++)
					{
						if (x - (blur_size / 2) + a >= 0 && x - (blur_size / 2) + a < w && y - (blur_size / 2) + b >= 0 && y - (blur_size / 2) + b < h)
						{
							values[a][b] = img[(y - (blur_size / 2) + b) * w + (x - (blur_size / 2) + a)];
						}
						else
						{
							values[a][b] = 0;
						}
					}
				}

				int sum = 0;
				int termNums = 0;

				for (int a = 0; a < blur_size; a++)
				{
					for (int b = 0; b < blur_size; b++)
					{
						sum += values[a][b];
						termNums++;
					}
				}

				int average = sum / termNums;
				out_img[y * w + x] = average;
			}
			else
			{
				out_img[y * w + x] = img[y * w + x];
			}
		}
	}

	return out_img;
}

unsigned char *threshold(unsigned char *img, int w, int h, int sum, double outerConstant, int threads)
{
	omp_set_num_threads(threads);

	unsigned char *out_img = new unsigned char[w * h];
	int threshold = (int)std::round(outerConstant * sum);

#pragma omp parallel for
	for (int x = 0; x < w; x++)
	{
		for (int y = starting_y; y < h; y++)
		{
			int currentValue = img[y * w + x];

			if (currentValue < threshold)
			{
				out_img[y * w + x] = 0;
			}
			else
			{
				out_img[y * w + x] = 255;
			}
		}
	}

	return out_img;
}
