# Structure Core Playground

An example of implementing a simple GUI to display output from a Structure Core sensor.

## Project Setup

This repository contains files directly from the [Structure SDK](https://structure.io/developers) for Linux, and has been tested on Ubuntu 18.04 (LTS) with a `x86_64` architecture. It may be ported to other platforms.

### Installing Dependencies

First, clone this repository:

```sh
git clone git@github.com:shantanubala/structure-gui.git
```

Then, install the dependencies:

```sh
sudo apt-get update
sudo apt-get install -y cmake build-essential libc6-dev libstdc++-5-dev libgl1-mesa-dev libegl1-mesa-dev libusb-1.0-0-dev libxcursor-dev libxinerama-dev libxrandr-dev
```

Enable driver access:

```sh
bash Driver/install-structure-core-linux.sh
```

## Building the Project

All of the build steps have been combined into a single script to create a build:

```sh
cd Scripts
bash build.sh
```

## Running the Project

Enter the directory for builds:

```sh
cd Builds/<config>-<arch>/Source
```

```sh
On release builds for Ubuntu, that would be:
```

```sh
cd Builds/release-x86_64/Source
```

You can re-compile this specific portion of the code by running the `Makefile`:

```sh
make
```

Or run the compiled binary:

```sh
./Playground
```