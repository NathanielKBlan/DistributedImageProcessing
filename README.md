# DistributedImageProcessing
Final for Parallel Computing

# Description
Distributed computing system for image processing

# Requirements
In order to run and build our distributed system one needs to have a C++17 compiler installed and [CMake](https://cmake.org/download/) (3.1 or greater)

# Running the Distributed System

First clone the project
```
git clone https://github.com/NathanielKBlan/DistributedImageProcessing.git
cd DistributedImageProcessing
```
Inside the DistributedImageProcessing directory should be two more directories /master and /worker
For each you will need to create a build and data folder

```
cd master
mkdir build
mkdir data
cd build
```
```
cd worker
mkdir build
mkdir data
cd build
```

Inside each build directory run CMake to generate a Makefile for the project.
Inside the data folder you can add the image you want processed, and is also where your output image will go

```
cmake ..
make
```

Now open up four terminals in the /worker/build directory and run a worker instance for the following ports 3000, 3001, 3002, 3003 (it's important that you use these 4 ports
```
./worker [port number]
``` 

Finally you can startup the master as so. It's important that you start the workers before the master, if the master can't find the workers it will exit.
```
./master ../data/[input image file name] ../data/[output image file name] [1 (for thresholding), 2 (for bluring), 3 (for upscaling)] chunks threads [blur_size/upscale amount]
```
Note: worker and master must be terminated using ^C if you want to use them again

Aslo Note: chunks should be a multiple of 4
