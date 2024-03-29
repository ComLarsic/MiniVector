#pragma once

#include "instruction.h"

#include <stdio.h>

#ifdef __unix__
#include <pthread.h>
#endif

#define BYTE_BUFFER_SIZE 8000

#ifdef _WIN32
#include <windows.h>
#endif

/**
 * @brief Pipe structure
 */
typedef struct
{
// The file descriptor for the pipe
#ifdef __unix__
    int32_t fd;
#endif
#ifdef _WIN32
    HANDLE fd;
#endif

    char *pipe_path;
    uint64_t index;                              // Points to where the next instruction starts
    mv_instruction_t *instruction_buffer;        // Buffer for the instruction
    mv_instruction_t *polled_instruction_buffer; // Buffer for the polled instructions. This is added to by the pipe thread and then copied into the instruction buffer
    uint64_t instruction_count;                  // Number of instructions in the buffer
    uint64_t polled_instruction_count;           // Number of instructions in the polled buffer
    uint8_t buffered_bytes[BYTE_BUFFER_SIZE];    // Buffer for the bytes
    uint32_t buffered_bytes_count;               // Number of bytes in the buffer

// The thread that reads from the pipe
#ifdef __unix__
    pthread_t thread;
    pthread_mutex_t instruction_buffer_lock;
#elif defined(_WIN32)
    HANDLE thread;
    HANDLE instruction_buffer_lock;
#endif

    // The number of instructions per frame
    uint64_t instructions_per_frame;
} mv_pipe;
/**
 * @brief Create a pipe
 *
 * @param pipe_path Path to the pipe
 * @param instructions_per_frame The number of instructions per frame
 */
mv_pipe *mv_create_pipe(char *pipe_path, uint64_t instructions_per_frame);

/**
 * @brief Destroy a pipe
 *
 * @param pipe Pipe to destroy
 */
void mv_destroy_pipe(mv_pipe *pipe);

/**
 * @brief Start the pipe thread
 *
 * @param pipe The pipe to start the thread for
 * @param instructions_per_frame The number of instructions per frame
 */
void mv_start_pipe_thread(mv_pipe *pipe);

/**
 * @brief Poll a pipe
 *
 * @param pipe Pipe to poll
 * @return The instruction read from the pipe
 * @param instructions_per_frame The number of instructions per frame
 */
void mv_poll_pipe(mv_pipe *pipe);

/**
 * @brief Read an instruction from the pipe
 *
 * @param pipe Pipe to read from
 */
mv_instruction_t *mv_read_instruction_from_pipe(mv_pipe *pipe);