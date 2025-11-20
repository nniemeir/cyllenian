# Cyllenian
A minimal HTTPS web server

## Dependencies
* GCC
* GNU make
* gzip
* OpenSSL development libraries

## Installation
Compile the project
```
make
```
Install the manpage and compiled binary
```
sudo make install
```

### Make Targets 
- `make` - Compile the binary
- `make install` – Copy binary and manpage to system directories
- `make clean` – Remove build objects
- `make fclean` - Remove build objects and binary

## Usage
```
cyllenian [OPTIONS]
```

### Options
```
-c <path>          Specify path to certificate file
-h                      Display program usage
-k <directory>          Specify path to private key file
-l                      Save program output to file
-p <port>               Specify port to listen on
```

## License
GNU General Public License V2

Copyright (c) 2025 Jacob Niemeir
