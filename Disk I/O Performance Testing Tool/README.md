# Disk I/O Performance Testing Tool

## Introduction

This program is written to perform disk I/O operations (read and write) with customizable parameters. It allows user to test the performance of disk operations by specifying parameters such as operation type (read/write), access pattern (sequential/random), request size, duration of test, and more. The program reports statistics like total duration, number of requests, data transferred, average latency, IO throughput, and bandwidth. Moreover, it stores the logs for each IO operation in trace file.

## Prerequisites

To compile and run this program, you need:
* A C++ compiler that supports C++11 or later (e.g., g++ version 4.8 or later)
* A POSIX-compliant operating system (e.g., Linux)

## Compilation Instructions 

Follow the following steps to compile the program. 

### Step 1

Move all the files into your working directory. 

### Step 2

On terminal, use the following commmand to compile the program

```
make
```

### Step 3

Verify if the compilation has been successfully completed by listing the directory content. New file named simplebench would appear in the list.

```
ls
```

## Running the Program

The program accepts several command-line arguments to customize the disk I/O operations and test according to that.

### Command-Line Arguments

* -e DURATION: Duration in seconds for which the test should run.
* -f DEVICE_OR_FILE: Path to the device or file on which the test will be performed.
* -r RANGE_MB: Range in megabytes within which the I/O operations will occur.
* -s REQUEST_SIZE_KB: Size of each I/O request in kilobytes.
* -t TYPE: Type of operation, either R for read or W for write.
* -p PATTERN: Access pattern, either S for sequential or R for random.
* -q QUEUE_DEPTH: Number of threads or queue depth (between 1 and 32).
* -d DIRECT_IO: Whether to use direct I/O, T for true or F for false.
* -o OUTPUT_TRACE: Output trace file path.

### Example Usage with command line 

Run the program with desired parameters as following:

```
simplebench -e 60 -f /dev/sda1 -r 64 -s 4 -t R -p S -q 2 -d T -o temp/output_trace.tr
```

Explanation of the parameters:

* -e 60: Run the test for 60 seconds.
* -f /dev/sda1: Use directory /dev/sda1 as the target for I/O operations.
* -r 64: Operate within a 64 MB range.
* -s 4: Use a request size of 4 KB.
* -t R: Perform read operations.
* -p S: Use sequential access pattern.
* -q 2: Set queue depth to 2 (2 threads).
* -d T: Use direct I/O.
* -o temp/output_trace.tr: Output trace file path.


## Output of Program 
The program will display statistics after execution:

```
Execution Completed
Total duration in seconds: 60.0023
Total number of requests: 15000
Total volume of data transferred in MBs: 58.5938
Average latency in milliseconds: 4.00015
Throughput in IOPS: 250
Bandwidth in MB/second: 0.976563
```

### Trace File 

The content in the trace file has the following format:

```
[Timestamp] [Thread ID] [Type] [Offset] [Size] [Latency]
```
* Timestamp: The issue time of I/O (in seconds).
* Thread ID: The ID of the thread generating this request
* Type: R (read) or W (write)
* Offset: The request offset (in unit of 512-byte sectors)
* Size: The request size (in number of sectors)
* Latency: The time of completing request (in milliseconds)

## Important Comments

* Ensure you have the necessary permissions to read/write to the specified file or device.

## Conclusion 

By following this guide, you should be able to compile and run the disk I/O test program. Remember to exercise caution when performing disk operations, especially on devices containing important data. Use test files whenever possible, and ensure you have the necessary permissions.

