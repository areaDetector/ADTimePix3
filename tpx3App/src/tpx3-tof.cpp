#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <json-c/json.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <new>

#include "ADTimePix.h"

#define TPX3_TDC_CLOCK_PERIOD_SEC ((1.5625 / 6.0) * 1e-9)
#define MAX_BUFFER_SIZE 32768  // Increased buffer size
#define MAX_BINS 1000

// Prerequisites
// apt-get install libjson-c-dev
// yum install libjson-c-devel

// Compile with:
// gcc -o tpx3-tof tpx3-tof.c -ljson-c

// Run with:
// ./tpx3-tof

// Create data directory
// mkdir -p data

/* TCP socket data format:
result: ({'timeAtFrame': 1741899848.247, 'frameNumber': 59, 'measurementID': None, 'dataSize': 40, 'bitDepth': 32, 'isPreviewSampled': True, 'thresholdID'
: 0, 'pixelEventNumber': 0, 'tdc1EventNumber': 0, 'tdc2EventNumber': 0, 'integrationSize': 0, 'integrationMode': None, 'binSize': 10, 'binWidth': 384000, 
'binOffset': 0, 'countPixels': 66684, 'countNoTdc': 0, 'countSum': 7380, 'countBelowMinimum': 0, 'countAboveMaximum': 59304, 'mean': 0.0, 'sigma': 0.0, 's
um': 0.0}, b'\x00\x00\x03L\x00\x00\x03\x1f\x00\x00\x03!\x00\x00\x03 \x00\x00\x03 \x00\x00\x03\x1f\x00\x00\x03 \x00\x00\x03!\x00\x00\x02\x18\x00\x00\x01\x9
0')
*/

/* Result:
Frame 60 data:
Bin edges: 0.000000000e+00 1.000000000e-04 2.000000000e-04 3.000000000e-04 4.000000000e-04 5.000000000e-04 6.000000000e-04 7.000000000e-04 8.000000000e-04 9.000000000e-04 1.000000000e-03 
Bin values: 844 800 800 799 801 800 800 800 536 400 
*/

// Structure to hold histogram data
typedef struct {
    double *bin_edges;
    union {
        uint32_t *bin_values_32;  // For individual frames
        uint64_t *bin_values_64;  // For running sum
    };
    int bin_size;
    int is_running_sum;  // Flag to indicate if this is a running sum
} HistogramData;

// Global running sum histogram
HistogramData *running_sum = NULL;

// Initialize running sum histogram
void init_running_sum(int bin_size, double *bin_edges) {
    printf("\nDebug - Creating new running sum structure\n");
    running_sum = new HistogramData;
    running_sum->bin_size = bin_size;
    running_sum->bin_edges = new double[bin_size + 1];
    running_sum->bin_values_64 = new uint64_t[bin_size];
    running_sum->is_running_sum = 1;  // Set the flag to indicate this is a running sum
    
    // Copy bin edges
    for (int i = 0; i < bin_size + 1; i++) {
        running_sum->bin_edges[i] = bin_edges[i];
    }
    
    // Initialize bin values to zero
    for (int i = 0; i < bin_size; i++) {
        running_sum->bin_values_64[i] = 0;
    }
    
    // Debug output
    printf("Debug - Initialized running sum:\n");
    for (int i = 0; i < bin_size; i++) {
        printf("Bin %d: %.9e\t%lu\n", i, running_sum->bin_edges[i], running_sum->bin_values_64[i]);
    }
}

// Add histogram data to running sum with overflow check
void add_to_running_sum(HistogramData *histo) {
    if (!running_sum) {
        printf("\nDebug - Initializing running sum with bin_size=%d\n", histo->bin_size);
        init_running_sum(histo->bin_size, histo->bin_edges);
    }
    
    // Debug output before addition
    printf("\nDebug - Adding to running sum:\n");
    for (int i = 0; i < histo->bin_size; i++) {
        printf("Bin %d: Current=%lu, Adding=%u\n", 
               i, running_sum->bin_values_64[i], histo->bin_values_32[i]);
    }
    
    // Add bin values with overflow check
    for (int i = 0; i < histo->bin_size; i++) {
        uint64_t new_value = running_sum->bin_values_64[i] + histo->bin_values_32[i];
        if (new_value < running_sum->bin_values_64[i]) {
            printf("Warning: Overflow detected in bin %d, capping at maximum value\n", i);
            running_sum->bin_values_64[i] = UINT64_MAX;
        } else {
            running_sum->bin_values_64[i] = new_value;
        }
    }
    
    // Debug output after addition
    printf("\nDebug - Running sum after addition:\n");
    for (int i = 0; i < histo->bin_size; i++) {
        printf("Bin %d: %lu\n", i, running_sum->bin_values_64[i]);
    }
}

// Function to create and connect socket
int create_client_socket(const char *host, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return -1;
    }

    // Set TCP_NODELAY to disable Nagle's algorithm
    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    // Set larger socket buffers
    int rcvbuf = 256 * 1024;  // 256KB receive buffer
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));  // Zero out the structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(sock);
        return -1;
    }

    printf("Attempting to connect to %s:%d...\n", host, port);
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return -1;
    }
    printf("Connected successfully\n");

    return sock;
}

// Calculate bin edges
double* calc_bin_edges(int bin_size, int bin_width, int bin_offset, int *edge_count) {
    *edge_count = bin_size + 1;
    double *bin_edges = new double[*edge_count];
    
    for (int i = 0; i < *edge_count; i++) {
        bin_edges[i] = (bin_offset + (i * bin_width)) * TPX3_TDC_CLOCK_PERIOD_SEC;
    }
    
    return bin_edges;
}

// Save histogram data to file
void save_histogram_data(const char *filename, HistogramData *histo) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror("Failed to open file");
        return;
    }

    // Set larger buffer for file I/O
    setvbuf(fp, NULL, _IOFBF, 65536);

    fprintf(fp, "# Time of Flight Histogram Data\n# Bins: %d\n#\n", histo->bin_size);

    // Debug output before saving
    printf("\nDebug - Saving to file %s:\n", filename);
    for (int i = 0; i < histo->bin_size; i++) {
        if (histo->is_running_sum) {
            printf("Bin %d: %.9e\t%lu\n", i, histo->bin_edges[i], histo->bin_values_64[i]);
            fprintf(fp, "%.9e\t%lu\n", histo->bin_edges[i], histo->bin_values_64[i]);
        } else {
            printf("Bin %d: %.9e\t%u\n", i, histo->bin_edges[i], histo->bin_values_32[i]);
            fprintf(fp, "%.9e\t%u\n", histo->bin_edges[i], histo->bin_values_32[i]);
        }
    }
    fprintf(fp, "%.9e\n", histo->bin_edges[histo->bin_size]);
    
    fclose(fp);
}

asynStatus ADTimePix::startHistogram() {
//    histogramThread = epicsThreadCreate("ADTimePix::histogram",
//                                        epicsThreadPriorityMedium,
//                                        epicsThreadGetStackSize(epicsThreadStackMedium),
//                                        (EPICSTHREADFUNC)ADTimePix::histogram,
//                                        this);
    histogramThread = std::thread(&ADTimePix::histogram, this);
    return asynSuccess;
}

asynStatus ADTimePix::stopHistogram() {
    this->acquiring_histogram = false;
    histogramThread.join();
    printf("Debug - Histogram thread joined\n");
    return asynSuccess;
}

// int main() {
// int histogram() {
asynStatus ADTimePix::histogram() {
    asynStatus status = asynSuccess;
    const char *server_ip = "localhost";
    int raw_port = 8451;
    
    // Create socket and connect
    int sock = create_client_socket(server_ip, raw_port);

    if (sock < 0) {
        return asynError;
//        return 1;
    }

    char *line_buffer = new char[MAX_BUFFER_SIZE];
    size_t total_read = 0;

    printf("Waiting for data...\n");
    while (this->acquiring_histogram) {
        ssize_t bytes_read = recv(sock, line_buffer + total_read, 
                                MAX_BUFFER_SIZE - total_read - 1, 0);

        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                printf("Connection closed by peer\n");
                this->acquiring_histogram = false;
                break;
            }
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("Socket error");
                this->acquiring_histogram = false;
                break;
            }
            continue;
        }

        total_read += bytes_read;
        line_buffer[total_read] = '\0';
        
        char *newline_pos = static_cast<char*>(memchr(line_buffer, '\n', total_read));
        if (newline_pos) {
            *newline_pos = '\0';
            
            // Parse JSON
            struct json_object *json_c = json_tokener_parse(line_buffer);
            if (json_c) {
                // Extract header information
                struct json_object *frame_number_obj, *bin_size_obj, *bin_width_obj, 
                                 *bin_offset_obj;
                
                json_object_object_get_ex(json_c, "frameNumber", &frame_number_obj);
                json_object_object_get_ex(json_c, "binSize", &bin_size_obj);
                json_object_object_get_ex(json_c, "binWidth", &bin_width_obj);
                json_object_object_get_ex(json_c, "binOffset", &bin_offset_obj);

                if (frame_number_obj && bin_size_obj && bin_width_obj && bin_offset_obj) {
                    int frame_number = json_object_get_int(frame_number_obj);
                    int bin_size = json_object_get_int(bin_size_obj);
                    int bin_width = json_object_get_int(bin_width_obj);
                    int bin_offset = json_object_get_int(bin_offset_obj);

                    // Allocate buffer for binary data
                    uint32_t *tof_bin_values = new uint32_t[bin_size];
                    size_t binary_needed = bin_size * sizeof(uint32_t);
                    size_t binary_read = 0;

                    // Copy any binary data we already have after the newline
                    size_t remaining = total_read - (newline_pos - line_buffer + 1);
                    if (remaining > 0) {
                        size_t to_copy = remaining > binary_needed ? binary_needed : remaining;
                        memcpy(tof_bin_values, newline_pos + 1, to_copy);
                        binary_read = to_copy;
                    }

                    // Read any remaining binary data needed
                    while (binary_read < binary_needed) {
                        ssize_t bytes = recv(sock, ((char*)tof_bin_values) + binary_read,
                                           binary_needed - binary_read, MSG_WAITALL);
                        if (bytes <= 0) {
                            perror("Failed to read binary data");
                            delete[] tof_bin_values;
                            json_object_put(json_c);
                            goto cleanup;
                        }
                        binary_read += bytes;
                    }

                    // Convert to little-endian
                    #pragma omp parallel for
                    for (int i = 0; i < bin_size; i++) {
                        tof_bin_values[i] = __builtin_bswap32(tof_bin_values[i]);
                    }

                    // Debug output for input values
                    printf("\nDebug - Input bin values:\n");
                    for (int i = 0; i < bin_size; i++) {
                        printf("Bin %d: %u\n", i, tof_bin_values[i]);
                    }

                    // Calculate bin edges first
                    int edge_count;
                    double *bin_edges = calc_bin_edges(bin_size, bin_width, bin_offset, &edge_count);

                    // Print bin values
                    printf("\nFrame %d data:\n", frame_number);
                    printf("Bin edges: ");
                    for (int i = 0; i < bin_size + 1; i++) {
                        printf("%.9e ", bin_edges[i]);
                    }
                    printf("\nBin values: ");
                    for (int i = 0; i < bin_size; i++) {
                        printf("%u ", tof_bin_values[i]);
                    }
                    printf("\n\n");

                    // Create histogram data structure
                    HistogramData histo = {
                        .bin_edges = bin_edges,
                        .bin_values_32 = tof_bin_values,
                        .bin_size = bin_size,
                        .is_running_sum = 0
                    };

                    // Debug output for running sum before addition
                    if (running_sum) {
                        printf("\nDebug - Running sum before addition:\n");
                        for (int i = 0; i < bin_size; i++) {
                            printf("Bin %d: %lu\n", i, running_sum->bin_values_64[i]);
                        }
                    }

                    // Add to running sum
                    add_to_running_sum(&histo);

                    // Debug output for running sum after addition
                    if (running_sum) {
                        printf("\nDebug - Running sum after addition:\n");
                        for (int i = 0; i < bin_size; i++) {
                            printf("Bin %d: %lu\n", i, running_sum->bin_values_64[i]);
                        }
                    }

                    // Save running sum
                    char filename[256];
                    snprintf(filename, sizeof(filename), "data/tof-histogram-running-sum.txt");
                    save_histogram_data(filename, running_sum);

                    printf("Frame %d processed (running sum updated)\n", frame_number);

                    delete[] bin_edges;
                    delete[] tof_bin_values;
                }
                
                json_object_put(json_c);
            }
            
            // Move any remaining data to start of buffer
            size_t remaining = total_read - (newline_pos - line_buffer + 1);
            if (remaining > 0) {
                memmove(line_buffer, newline_pos + 1, remaining);
            }
            total_read = remaining;
        }
        
        // Prevent buffer overflow
        if (total_read >= MAX_BUFFER_SIZE - 1) {
            printf("Buffer full, resetting\n");
            total_read = 0;
        }
    }

cleanup:
    // Cleanup running sum
    if (running_sum) {
        delete[] running_sum->bin_edges;
        delete[] running_sum->bin_values_64;
        delete running_sum;
    }
    delete[] line_buffer;
    close(sock);
    printf("\n*** Ready ****\n");
    this->acquiring_histogram = false;
    return status;
}
