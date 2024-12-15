// Including packages

#include <iostream>
#include <fcntl.h>  
#include <unistd.h>  
#include <cstdlib>  
#include <cstring>    
#include <chrono>
#include <random>
#include <vector>
#include <cctype>
#include <algorithm>
#include <thread>
#include <fstream>
#include <iomanip>
#include <string>
#include <mutex>
#include <filesystem>



//mutex for synchronization to access certain variables
std::mutex traceMutex;
std::mutex outputMutex;


/**
 * @brief A class for parsing command-line arguments.
 * 
 * The InputParser class is designed to simplify the process of parsing 
 * and retrieving values from command-line arguments passed to a program.
 * It tokenizes the command-line arguments and provides methods to 
 * query specific options and their associated values.
 */
class InputParser {
public:
    InputParser(int& argc, char** argv) {
        for (int i = 1; i < argc; ++i)
            this->tokens.push_back(std::string(argv[i]));
    }

    const std::string& getCmdOption(const std::string& option) const {
        auto itr = std::find(this->tokens.begin(), this->tokens.end(), option);
        if (itr != this->tokens.end() && ++itr != this->tokens.end()) {
            return *itr;
        }
        static const std::string empty_string("");
        return empty_string;
    }

private:
    std::vector<std::string> tokens;
};

/**
 * @brief Inserts a formatted string representing an I/O operation into a trace vector.
 * 
 * This function formats the details of an I/O operation, including its timestamp,
 * thread ID, operation type, offset, size, and latency, into a string. The formatted 
 * string is then inserted into the provided trace vector. This trace vector can be 
 * used later to write a trace file for analysis.
 * 
 * @param traceVector Pointer to a vector of strings where trace entries are stored.
 * @param timestamp The starting time of the I/O operation, in seconds.
 * @param threadID The ID of the thread performing the I/O operation.
 * @param type The type of I/O operation ('R' for read, 'W' for write, etc.).
 * @param offset The offset (in bytes) of the I/O operation in the file/device.
 * @param size The size (in bytes) of the I/O operation.
 * @param latency The latency (in seconds) of the I/O operation.
 */ 
void insertIntoTrace(std::vector<std::string>* traceVector, double timestamp, int threadID, char type, int offset, int size, double latency) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6)
        << timestamp << " "
        << threadID << " "
        << type << " "
        << offset << " "
        << size << " "
        << latency;
    std::lock_guard<std::mutex> lock(traceMutex);
    traceVector->push_back(oss.str());
}


/**
 * @brief Writes the contents of a vector to a trace file.
 * 
 * This function takes a vector of strings and writes each element as a new line 
 * in the specified trace file. If the file cannot be opened, an error message is 
 * displayed on the standard error stream.
 * 
 * @param traceVector The vector of strings to write to the file. Each string 
 *                    represents one line in the output file.
 * @param filename The name (or path) of the file to write the vector contents to.
 * 
 * @note The function overwrites the contents of the file if it already exists.
 *       Ensure you have proper write permissions for the specified file.
 */
void writeVectorToFile(const std::vector<std::string>& traceVector, const std::string& filename) {
    std::ofstream outFile(filename, std::ios::out);
    if (!outFile.is_open()) {
        std::cerr << "Error: Unable to open file " << filename << std::endl;
        return;
    }
    for (const auto& line : traceVector) {
        outFile << line << "\n";
    }
    outFile.close();
}

/**
 * @brief Performs a write operation on a specified file or disk with various configurations.
 * 
 * This function executes write operations for a given duration and tracks performance metrics.
 * It supports both direct I/O and buffered I/O modes and can write data in sequential or random patterns.
 * The function logs trace data for each write operation, including latency, thread ID, and offset.
 * 
 * @param disk_file The file or disk path on which the write operation will be performed.
 * @param duration The duration (in seconds) for which the write operation will run.
 * @param memory_MB The size of the memory range (in MB) within which the I/O operations will occur.
 * @param size_KB The size of each write request (in KB).
 * @param pattern The write access pattern ('S' for sequential, 'R' for random).
 * @param direct_io A boolean indicating whether to use direct I/O (`true`) or buffered I/O (`false`).
 * @param program_start_time The start time of the program, used for calculating total elapsed time.
 * @param tracePntr Pointer to a vector that stores trace data for each write operation.
 * @param latencyPtr Pointer to a variable that accumulates the average latency of write operations.
 * @param threadID The ID of the thread performing the write operation.
 * @param total_IO Pointer to a variable that accumulates the total number of I/O operations.
 * 
 * @return `true` if the write operation completes successfully, `false` if an error occurs.
 * 
 * @note 
 * - Function is not using mutex or any other lock to access and write to specific portion of file. 
 * It is because of the reason that our purpose is to measure the maximum performance, and we are not conerned 
 * about the inconsistency in data as it is dummy data. Synchronization may serialize the operations in multithreads
 * scenario and may not simulate the scenario where multiple threads writes into same devices at different positions. 
 */

bool writeOperation(std::string disk_file, int duration, int memory_MB, int size_KB, char pattern, bool direct_io,  std::chrono::high_resolution_clock::time_point program_start_time, std::vector<std::string>* tracePntr,double* latencyPtr, int threadID, int* total_IO) {

    // Variable initialization
    int IO_count = 0;
    int starting_offset = 0;  // It can change in some cases within the disk range
    const size_t block_size = size_KB * 1024;
    const size_t memory_range = memory_MB * 1024 * 1024;
    std::random_device rd;
    std::mt19937 gen(rd());
    int randomGenRange = (memory_range - block_size + starting_offset) / 512;
    std::uniform_int_distribution<> distrib(starting_offset, randomGenRange);

    // Initializing variables for time duration
    auto startTime = std::chrono::high_resolution_clock::now();
    auto endTime = startTime;
    std::chrono::duration<double> elapsedTime = endTime - startTime;
	std::chrono::duration<double> totalTime=endTime - startTime;
    double elapsedSeconds = elapsedTime.count();
	double total_time;
    double latency=0;

    // Misc. variables 
    const char* disk_file_name = disk_file.c_str();
	off_t offset_b=0;


    if (direct_io) {

        int fd = open(disk_file_name, O_WRONLY | O_DIRECT);
        if (fd < 0) {
            perror("Error opening file with O_DIRECT");
            return false;
        }

        void* buffer;
        size_t alignment = 512;
        if (posix_memalign(&buffer, alignment, block_size) != 0) {
            perror("Error allocating aligned buffer");
            close(fd);
            return false;
        }
        memset(buffer, 'A', block_size);

        off_t offset = block_size;

        off_t new_offset;
        
        while (total_time < duration) {
            startTime = std::chrono::high_resolution_clock::now();
			
            if (pattern == 'R') {
				offset_b=distrib(gen);
                offset =  offset_b* 512;
                new_offset = lseek(fd, offset, SEEK_SET);
            }
            else {
				offset_b+=(block_size/512);
                new_offset = lseek(fd, offset, SEEK_CUR);
            }

            if (new_offset == (off_t)-1) {
                perror("Error seeking in file");
                free(buffer);
                close(fd);
                return false;
            }
            if (new_offset < starting_offset || new_offset+block_size > memory_range) {
                if (lseek(fd, starting_offset, SEEK_SET) == (off_t)-1) {
                    perror("lseek reset failed");
                }
                else {
					offset_b=0;
                }
            }
            ssize_t bytes_written = write(fd, buffer, block_size);
            if (bytes_written != (ssize_t)block_size) {
                perror("Error writing to file");
            }
            else {
                // Do nothing.
            }
			
            endTime = std::chrono::high_resolution_clock::now();
            totalTime=startTime-program_start_time;
            elapsedTime = endTime - startTime;
            elapsedSeconds = elapsedTime.count();
			total_time=totalTime.count();
			insertIntoTrace(tracePntr,total_time , threadID, pattern, offset_b, block_size/512, elapsedSeconds*1000);
            total_time=total_time+elapsedSeconds;
            latency+=elapsedSeconds;
            IO_count++;
        }
        free(buffer);
        close(fd);
    }
    else {
		int fd = open(disk_file_name, O_WRONLY);
		if (fd < 0) {
			perror("Error opening file");
			return false;
		}
        void* buffer = malloc(block_size);
        if (!buffer) {
            perror("Error allocating buffer");
            close(fd);
            return false;
        }

        memset(buffer, 'A', block_size);
        off_t offset = block_size;
        off_t new_offset;

        
        while (total_time < duration) {
			startTime = std::chrono::high_resolution_clock::now();
            if (pattern == 'R') {
				offset_b=distrib(gen);
                offset =  offset_b* 512;
                new_offset = lseek(fd, offset, SEEK_SET);
            }
            else {
				offset_b+=(block_size/512);
                new_offset = lseek(fd, offset, SEEK_CUR);
            }

            if (new_offset == (off_t)-1) {
                perror("Error seeking in file");
                free(buffer);
                close(fd);
                return false;
            }
            if (new_offset < starting_offset || new_offset+block_size > memory_range) {
                if (lseek(fd, starting_offset, SEEK_SET) == (off_t)-1) {
                    perror("lseek reset failed");
                }
                else {
					offset_b=0;
                }
            }
            ssize_t bytes_written = write(fd, buffer, block_size);
            if (bytes_written != (ssize_t)block_size) {
                perror("Error writing to file");
            }
            else {
                // Do Nothing.
            }
            endTime = std::chrono::high_resolution_clock::now();
            totalTime=startTime-program_start_time;
            elapsedTime = endTime - startTime;
            elapsedSeconds = elapsedTime.count();
			total_time=totalTime.count();
			insertIntoTrace(tracePntr,total_time , threadID, pattern, offset_b, block_size/512, elapsedSeconds*1000);
            total_time=total_time+elapsedSeconds;
            latency+=elapsedSeconds;
            IO_count++;
        }
        free(buffer);
        close(fd);
    }

    std::lock_guard<std::mutex> lock(outputMutex);
    *total_IO +=IO_count;
    *latencyPtr+=double(latency)/IO_count;

    return true;
}

/**
 * @brief Performs a read operation on a specified file or disk with various configurations.
 * 
 * This function executes read operations for a given duration and collects performance metrics.
 * It supports both direct I/O and buffered I/O modes and allows for sequential or random access patterns.
 * Detailed trace data for each read operation, such as latency, thread ID, and offset, is logged.
 * 
 * @param disk_file The file or disk path on which the read operation will be performed.
 * @param duration The duration (in seconds) for which the read operation will run.
 * @param memory_MB The size of the memory range (in MB) within which the I/O operations will occur.
 * @param size_KB The size of each read request (in KB).
 * @param pattern The read access pattern ('S' for sequential, 'R' for random).
 * @param direct_io A boolean indicating whether to use direct I/O (`true`) or buffered I/O (`false`).
 * @param program_start_time The start time of the program, used for calculating total elapsed time.
 * @param tracePntr Pointer to a vector that stores trace data for each read operation.
 * @param latencyPtr Pointer to a variable that accumulates the average latency of read operations.
 * @param threadID The ID of the thread performing the read operation.
 * @param total_IO Pointer to a variable that accumulates the total number of I/O operations.
 * 
 * @return `true` if the read operation completes successfully, `false` if an error occurs.

 */
bool readOperation(std::string disk_file, int duration, int memory_MB, int size_KB, char pattern, bool direct_io,  std::chrono::high_resolution_clock::time_point program_start_time, std::vector<std::string>* tracePntr, double* latencyPtr,int threadID, int* total_IO) {
    // Variable initialization for stats
    int IO_count = 0;
    int starting_offset = 0;  // It can change in some cases within the disk range

    const size_t block_size = size_KB * 1024;
    const size_t memory_range = memory_MB * 1024 * 1024;

    // Initialization for random offset generation
    std::random_device rd;
    std::mt19937 gen(rd());
    int randomGenRange = (memory_range - block_size + starting_offset) / 512;
    std::uniform_int_distribution<> distrib(starting_offset, randomGenRange);

    // Initializing variables for time duration
    auto startTime = std::chrono::high_resolution_clock::now();
    auto endTime = startTime;
    std::chrono::duration<double> elapsedTime = endTime - startTime;
	std::chrono::duration<double> totalTime=endTime - startTime;
    double elapsedSeconds = elapsedTime.count();
	double total_time;
	off_t offset_b=0;
    double latency=0;
	
	
    // Misc. variables
    const char* disk_file_name = disk_file.c_str();


    if (direct_io) {

        int fd = open(disk_file_name, O_RDONLY | O_DIRECT);
        if (fd < 0) {
            perror("Error opening file with O_DIRECT");
            return false;
        }

        void* buffer;
        size_t alignment = 512; 
        if (posix_memalign(&buffer, alignment, block_size) != 0) {
            perror("Error allocating aligned buffer");
            close(fd);
            return false;
        }

        off_t offset = block_size;
        off_t new_offset;

        while (total_time < duration) {
			startTime = std::chrono::high_resolution_clock::now();
            if (pattern == 'R') {
				offset_b=distrib(gen);
                offset =  offset_b* 512;
                new_offset = lseek(fd, offset, SEEK_SET);
            }
            else {
				offset_b+=(block_size/512);
                new_offset = lseek(fd, offset, SEEK_CUR);
            }

            if (new_offset == (off_t)-1) {
                perror("Error seeking in file");
                free(buffer);
                close(fd);
                return false;
            }
            if (new_offset < starting_offset || new_offset+block_size > memory_range) {
                if (lseek(fd, starting_offset, SEEK_SET) == (off_t)-1) {
                    perror("lseek reset failed");
                }
                else {
					offset_b=0;
                }
            }

            ssize_t bytes_read = read(fd, buffer, block_size);
            if (bytes_read != (ssize_t)block_size) {
                perror("Error reading from file");
            }
            else {
                // Do Nothing.
            }
            endTime = std::chrono::high_resolution_clock::now();
            totalTime=startTime-program_start_time;
            elapsedTime = endTime - startTime;
            elapsedSeconds = elapsedTime.count();
			total_time=totalTime.count();
			insertIntoTrace(tracePntr,total_time , threadID, pattern, offset_b, block_size/512, elapsedSeconds*1000);
            total_time=total_time+elapsedSeconds;
            latency+=elapsedSeconds;
            IO_count++;
        }
        free(buffer);
        close(fd);
    }
    else {
		int fd = open(disk_file_name, O_RDONLY);
		if (fd < 0) {
			perror("Error opening file");
			return false;
		}
        void* buffer = malloc(block_size);
        if (!buffer) {
            perror("Error allocating buffer");
            close(fd);
            return false;
        }

        off_t offset = block_size;
        off_t new_offset;

        
        while (total_time < duration) {
			startTime = std::chrono::high_resolution_clock::now();
            if (pattern == 'R') {
				offset_b=distrib(gen);
                offset =  offset_b* 512;
                new_offset = lseek(fd, offset, SEEK_SET);
            }
            else {
				offset_b+=(block_size/512);
                new_offset = lseek(fd, offset, SEEK_CUR);
            }

            if (new_offset == (off_t)-1) {
                perror("Error seeking in file");
                free(buffer);
                close(fd);
                return false;
            }
            if (new_offset < starting_offset || new_offset+block_size > memory_range) {
                if (lseek(fd, starting_offset, SEEK_SET) == (off_t)-1) {
                    perror("lseek reset failed");
                }
                else {
					offset_b=0;
                }
            }

            ssize_t bytes_read = read(fd, buffer, block_size);
            if (bytes_read != (ssize_t)block_size) {
                perror("Error reading from file");
            }
            else {
                // Do Nothing.
            }
            endTime = std::chrono::high_resolution_clock::now();
            totalTime=startTime-program_start_time;
            elapsedTime = endTime - startTime;
            elapsedSeconds = elapsedTime.count();
			total_time=totalTime.count();
			insertIntoTrace(tracePntr,total_time , threadID, pattern, offset_b, block_size/512, elapsedSeconds*1000);
            total_time=total_time+elapsedSeconds;
            latency+=elapsedSeconds;
            IO_count++;
        }
        free(buffer);
        close(fd);
    }



    std::lock_guard<std::mutex> lock(outputMutex);
    *total_IO +=IO_count;
    *latencyPtr+=double(latency)/IO_count;
    return true;
}

/**
 * @brief Creates a dummy file with a specified size, filled with placeholder data.
 * 
 * This function generates a file of a specified size (in MB) and fills it with 
 * placeholder data (character 'A'). The file can be used as a test file for I/O operations.
 * 
 * @param dataSizeMB The size of the dummy file to create, in megabytes.
 * @param filePath The path where the dummy file will be created.
 * 
 * @return `true` if the file is created successfully, `false` if an error occurs (e.g., unable to open the file for writing).
 */

bool createDummyFile(int dataSizeMB, std::string filePath){

    const size_t dataSizeBytes = dataSizeMB * 1024 * 1024;
    std::vector<char> dataBuffer(dataSizeBytes, 'A');
    std::ofstream outputFile(filePath, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!outputFile.is_open()) {
        std::cerr << "Error opening file for writing: " << filePath << std::endl;
        return false;
    }
    outputFile.write(dataBuffer.data(), dataBuffer.size());
    outputFile.flush();
    outputFile.close();

    return true;
}

/**
 * @brief Entry point of the program, responsible for parsing command-line arguments, 
 * initializing resources, and executing read or write operations with multiple threads.
 * 
 * This `main` function processes user input provided through command-line arguments, validates 
 * them, and initializes variables for disk I/O operations. It supports both read and write 
 * operations, sequential or random access patterns, and direct or buffered I/O modes. The 
 * program operates with multithreading and logs detailed traces of operations to a specified 
 * output file.
 * 
 * Command-line options:
 * - `-e`: Duration (in seconds) for which the operations will run (required).
 * - `-f`: Path to the device or file for I/O operations (required).
 * - `-r`: Range (in MB) within which operations will occur (required).
 * - `-s`: Size of each request (in KB) (required).
 * - `-t`: Type of operation, `R` for read or `W` for write (required).
 * - `-p`: Access pattern, `S` for sequential or `R` for random (required).
 * - `-q`: Number of threads (queue depth), between 1 and 32 (required).
 * - `-d`: Direct I/O mode, `T` for true or `F` for false (required).
 * - `-o`: Path to the output file for traces (required).
 * 
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line arguments.
 * 
 * @return Returns `0` on successful execution, or `1` if an error occurs.
 */

int main(int argc, char** argv) {
    InputParser input(argc, argv);

    // Variables for argument parameters
    int duration = 0; 
    std::string device_or_file;
    int range_mb = 0;
    int request_size_kb = 0;
    char type = 'R';
    char pattern = 'S';
    int num_threads = 1;
    bool direct_io = false; 
    std::string output_trace;
	
	// Other Variables
    std::string traces_output;
	std::vector<std::string> traces;
	std::vector<std::string>* tracePntr = &traces;
    double latency=0;
    int total_IO=0;
    std::vector<std::thread> threads;
	
    // Parse -e
    const std::string& e_value = input.getCmdOption("-e");
    if (!e_value.empty()) {
        duration = std::atoi(e_value.c_str());
    }
    else {
        std::cerr << "Error: Missing argument for -e\n";
        return 1;
    }

    // Parse -f
    const std::string& f_value = input.getCmdOption("-f");
    if (!f_value.empty()) {
        device_or_file = f_value;

    }
    else {
        std::cerr << "Error: Missing argument for -f\n";
        return 1;
    }

    // Parse -r
    const std::string& r_value = input.getCmdOption("-r");
    if (!r_value.empty()) {
        range_mb = std::atoi(r_value.c_str());
    }
    else {
        std::cerr << "Error: Missing argument for -r\n";
        return 1;
    }

    // Parse -s
    const std::string& s_value = input.getCmdOption("-s");
    if (!s_value.empty()) {
        request_size_kb = std::atoi(s_value.c_str());
    }
    else {
        std::cerr << "Error: Missing argument for -s\n";
        return 1;
    }

    // Parse -t
    const std::string& t_value = input.getCmdOption("-t");
    if (!t_value.empty()) {
        type = std::toupper(t_value[0]);
        if (type != 'R' && type != 'W') {
            std::cerr << "Error: Invalid argument for -t (must be 'R' or 'W')\n";
            return 1;
        }
    }
    else {
        std::cerr << "Error: Missing argument for -t\n";
        return 1;
    }

    // Parse -p
    const std::string& p_value = input.getCmdOption("-p");
    if (!p_value.empty()) {
        pattern = std::toupper(p_value[0]);
        if (pattern != 'S' && pattern != 'R') {
            std::cerr << "Error: Invalid argument for -p (must be 'S' or 'R')\n";
            return 1;
        }
    }
    else {
        std::cerr << "Error: Missing argument for -p\n";
        return 1;
    }

    // Parse -q
    const std::string& q_value = input.getCmdOption("-q");
    if (!q_value.empty()) {
        num_threads = std::atoi(q_value.c_str());
        if (num_threads < 1 || num_threads > 32) {
            std::cerr << "Error: Invalid argument for -q (must be between 1 and 32)\n";
            return 1;
        }
    }
    else {
        std::cerr << "Error: Missing argument for -q\n";
        return 1;
    }

    // Parse -d
    const std::string& d_value = input.getCmdOption("-d");
    if (!d_value.empty()) {
        char d = std::toupper(d_value[0]);
        if (d == 'T') {
            direct_io = true;
        }
        else if (d == 'F') {
            direct_io = false;
        }
        else {
            std::cerr << "Error: Invalid argument for -d (must be 'T' or 'F')\n";
            return 1;
        }
    }
    else {
        std::cerr << "Error: Missing argument for -d\n";
        return 1;
    }

    // Parse -o
    const std::string& o_value = input.getCmdOption("-o");
    if (!o_value.empty()) {
        output_trace = o_value;
    }
    else {
        std::cerr << "Error: Missing argument for -o\n";
        return 1;
    }

// checking file path 
    std::filesystem::path fspath(device_or_file);
    if(std::filesystem::exists(fspath)){
        if(std::filesystem::is_directory(fspath)){
            std::string file="testfile.bin";
            device_or_file=device_or_file+"/"+file;
            bool f=createDummyFile(range_mb, device_or_file);
            if(f!=true){
                std::cerr << "Error: Error in file\n";
                return 1;
            }
        }
        else{
                ///
        }
    }
    else{
        std::cerr << "Error: Path does not exist.\n";
        return 1;    
    }


    //
    // Code Logic Begins Here
    //

	auto program_start_time = std::chrono::high_resolution_clock::now();
    auto operation = [&](int thread_id) {
        bool complete = false;
        if (type == 'W') {
            complete = writeOperation(device_or_file, duration, range_mb, request_size_kb, pattern, direct_io,program_start_time, tracePntr,&latency,thread_id,&total_IO );
        } else {
            complete = readOperation(device_or_file, duration, range_mb, request_size_kb, pattern, direct_io,program_start_time, tracePntr,&latency,thread_id,&total_IO);
        }

        if (complete) {
            //std::cout << "Thread " << thread_id << ": Execution Completed" << std::endl;
        } else {
            std::cout << "Thread " << thread_id << ": Execution not complete due to some error." << std::endl;
        }
    };

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(operation, i);
    }

    for (auto& t : threads) {
        t.join();
    }
	writeVectorToFile(traces, output_trace);

        // Output statistics
    std::cout << "Execution Completed" << std::endl;
    std::cout << "Total duration in seconds: " << duration << std::endl;
    std::cout << "Total number of requests: " << total_IO << std::endl;
    std::cout << "Total volume of data transferred in MBs: " << (total_IO * request_size_kb) / 1024 << std::endl;
    std::cout << "Average latency in milliseconds: " << (latency/num_threads) * 1000 << std::endl;
    std::cout << "Throughput in IOPS: " << float(total_IO) / duration << std::endl;
    std::cout << "Bandwidth in MB/second: " << (float(total_IO * request_size_kb) / 1024) / duration << std::endl;
    return 0;
}
