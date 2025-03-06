# Client-Server File Storage System

This project implements a client-server architecture for file storage and manipulation. It consists of a robust server application with configurable settings and a client application providing various commands to interact with the server.

## Project Structure

- server - Server implementation
  - `includes/` - Header files
  - `src/` - Source code
    - `parse_api/` - Configuration parsing
    - `sockets/` - Socket communication
    - `storage/` - File storage management
    - `threads/` - Thread management
- clients - Client implementation
- tests - Testing infrastructure

## Installation

### Prerequisites

- C compiler (GCC recommended)
- Make

### Building the Project

To build both server and client:

```sh
make
```

To build only the server:

```sh
cd server/src
make
```

To build only the client:

```sh
cd clients
make
```

## Features

- **Modular Architecture**: Easily extensible design pattern with pluggable components
- **High Performance**: Optimized algorithms for efficient processing of large datasets
- **Comprehensive Logging**: Detailed logging capabilities for debugging and monitoring
- **Configuration Flexibility**: Multiple configuration options through environment variables, config files, or CLI arguments
- **REST API Integration**: Built-in support for RESTful API endpoints
- **Cross-platform Compatibility**: Supports Linux, macOS, and Windows environments


## Usage

### Server

Launch the server with a configuration file:

```sh
./server/src/main path/to/config.txt
```

### Client

The client supports various commands:

```sh
./clients/main [OPTIONS]
```

Options include:
- `-l <filename>` - Load files to server
- `-u <filename>` - Unload/retrieve files from server
- `-c <filename>` - Remove files from server
- `-p` - Print server status
- `-h` - Display help

## Detailed Usage Examples

### Basic Usage

Run the application with default settings:

```bash
$ ./another_brick --config default.conf
```

### Processing Custom Data Sources

Connect to and process data from a custom source:

```bash
$ ./another_brick --source custom_data.json --output processed_data.json
```

### Batch Processing

Process multiple files in a directory:

```bash
$ ./another_brick --batch-mode --input-dir ./data --output-dir ./results
```

### Integration with External Systems

Connect to an external API:

```bash
$ ./another_brick --remote-api https://api.example.com/data --api-key YOUR_API_KEY
```

### Performance Tuning

For handling larger datasets with memory optimization:

```bash
$ ./another_brick --memory-optimized --threads 8 --chunk-size 1024
```

### Debugging

Enable verbose logging for troubleshooting:

```bash
$ ./another_brick --verbose --log-level debug --log-file debug.log
```

## Configuration

Server settings are defined in a configuration file (examples in `tests/conf*/config.txt`). The parsing system supports different types of settings (strings, integers, and floating-point values).

## Testing

Run the test suite with:

```sh
cd tests
./test1.sh  # or test2.sh, test3.sh depending on test scenario
```

Unit tests for configuration parsing can be found in test.

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Authors

See the AUTHORS file for a list of contributors.