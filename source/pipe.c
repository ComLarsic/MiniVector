#include "pipe.h"
#include "legacy/display.h"
#include "log.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

/**
 * @brief Create a pipe
 *
 * @param pipe_path Path to the pipe
 * @param instructions_per_frame The number of instructions per frame
 */
mv_pipe *mv_create_pipe(char *pipe_path, uint64_t instructions_per_frame)
{
    MV_INFO("Setting up pipe");
    mv_pipe *pipe = malloc(sizeof(mv_pipe));
    pipe->pipe_path = pipe_path;
    pipe->index = 0;
    pipe->instruction_buffer = malloc(sizeof(mv_instruction_t));
    pipe->polled_instruction_buffer = malloc(sizeof(mv_instruction_t));
    pipe->polled_instruction_count = 0;
    pipe->instruction_count = 0;
    pipe->buffered_bytes_count = 0;
    pipe->instructions_per_frame = instructions_per_frame;

// Create the pipe
#ifdef __unix__
    mkfifo(pipe->pipe_path, 0666);
    pipe->fd = open(pipe->pipe_path, O_RDWR | O_NONBLOCK);
#elif defined(_WIN32)
    pipe->fd = CreateNamedPipe(pipe->pipe_path, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 0, 0, 0, NULL);
    if (pipe->fd == INVALID_HANDLE_VALUE)
    {
        MV_ERROR("Failed to create the pipe\n");
        exit(1);
    }
    ConnectNamedPipe(pipe->fd, NULL);
#endif

    return pipe;
}

/**
 * @brief Destroy a pipe
 *
 * @param pipe Pipe to destroy
 */
void mv_destroy_pipe(mv_pipe *pipe)
{
    pthread_join(pipe->thread, NULL);
    MV_TRACE("Destroying pipe %s\n", pipe->pipe_path);
    // delete the pipe
    remove(pipe->pipe_path);
    free(pipe);
}

/**
 * @brief Start the pipe thread
 *
 * @param pipe The pipe to start the thread for
 * @param instructions_per_frame The number of instructions per frame
 */
void mv_start_pipe_thread(mv_pipe *pipe)
{
    // Create the thread that reads from the pipe
    int32_t thread_create_result = pthread_create(&pipe->thread, NULL, (void *)mv_poll_pipe, pipe);
    if (thread_create_result)
    {
        MV_ERROR("Failed to create the pipe thread\n");
        exit(1);
    }

    int32_t mutex_init_result = pthread_mutex_init(&pipe->instruction_buffer_lock, NULL);
    if (mutex_init_result)
    {
        MV_ERROR("Failed to create the pipe mutex\n");
        exit(1);
    }
}

/**
 * @brief Poll a pipe
 *
 * @param pipe Pipe to poll
 * @return The instruction read from the pipe
 */
void mv_poll_pipe(mv_pipe *pipe)
{
#ifdef __unix__
    typedef uint8_t word_t;
#elif defined(_WIN32)
    typedef DWORD word_t;
#endif

    word_t instructions_buffer_size = 5;
    const int8_t instructions_buffer_increment = 10;
    word_t bytes_read = 0;

    static uint8_t last_any_read = 0;
    int8_t any_read = 0;

    int8_t buffer[BYTE_BUFFER_SIZE];

    while (1)
    {
#ifdef __unix__
        any_read = 0;
        if ((bytes_read = read(pipe->fd, buffer, sizeof(uint8_t))) > 0)
        {
            for (word_t i = 0; i < bytes_read; i++)
            {
                if (pipe->buffered_bytes_count >= BYTE_BUFFER_SIZE)
                {
                    MV_ERROR("Byte buffer overflow\n");
                    exit(1);
                }
                pipe->buffered_bytes[pipe->buffered_bytes_count] = buffer[i];
                pipe->buffered_bytes_count++;
            }
            any_read = 1;
        }
#elif defined(_WIN32)
        any_read = 0;
        if (ReadFile(pipe->fd, buffer, sizeof(uint8_t), &bytes_read, NULL))
        {
            for (word_t i = 0; i < bytes_read; i++)
            {
                if (pipe->buffered_bytes_count >= BYTE_BUFFER_SIZE)
                {
                    MV_ERROR("Byte buffer overflow\n");
                    exit(1);
                }
                pipe->buffered_bytes[pipe->buffered_bytes_count] = buffer[i];
                pipe->buffered_bytes_count++;
            }
            any_read = 1;
        }
#endif

        if ((pipe->polled_instruction_count >= pipe->instructions_per_frame) || !any_read && !last_any_read)
        {
            if (pipe->polled_instruction_count <= 0)
            {
                continue;
            }

            while (pthread_mutex_lock(&pipe->instruction_buffer_lock) != 0)
            {
            }

            // If the instructions have reached the instructions per frame, copy the polled instructions into the instruction buffer
            pipe->instruction_buffer = realloc(pipe->instruction_buffer, (pipe->instruction_count + pipe->polled_instruction_count) * sizeof(mv_instruction_t));
            for (int64_t i = 0; i < pipe->polled_instruction_count; i++)
            {
                pipe->instruction_buffer[pipe->instruction_count++] = pipe->polled_instruction_buffer[i];
            }
            pipe->polled_instruction_count = 0;

            pthread_mutex_unlock(&pipe->instruction_buffer_lock);
        }

        last_any_read = any_read;

        // If the amount of bytes in the buffer is big enough to contain an instruction
        // Read the instruction, and remove it from the buffer
        while (pipe->buffered_bytes_count >= MV_INSTRUCTION_SIZE)
        {
            // Reallocation of the instruction buffer
            pipe->polled_instruction_buffer = realloc(pipe->polled_instruction_buffer, (pipe->polled_instruction_count + 1) * sizeof(mv_instruction_t));
            if (pipe->polled_instruction_buffer == NULL)
            {
                MV_ERROR("Failed to reallocate the polled instruction buffer\n");
                exit(1);
            }
            mv_instruction_t instruction = (mv_instruction_t){
                .type = pipe->buffered_bytes[0],
                .data = (pipe->buffered_bytes[1] << 24) | (pipe->buffered_bytes[2] << 16) | (pipe->buffered_bytes[3] << 8) | pipe->buffered_bytes[4]};

            pipe->polled_instruction_buffer[pipe->polled_instruction_count++] = instruction;

            // TRACE("Read instruction(%d): %d, %d\n", pipe->polled_instruction_count, instruction.type, instruction.data);
            // and move the rest of the buffer to the start
            for (int8_t i = 0; i < pipe->buffered_bytes_count - MV_INSTRUCTION_SIZE; i++)
            {
                if (i + MV_INSTRUCTION_SIZE < BYTE_BUFFER_SIZE)
                {
                    continue;
                }
                pipe->buffered_bytes[i] = pipe->buffered_bytes[i + MV_INSTRUCTION_SIZE];
            }
            pipe->buffered_bytes_count -= MV_INSTRUCTION_SIZE;

            // If the instruction buffer size is bigger than the max of int64_t, break
            if (pipe->polled_instruction_count >= INT64_MAX)
            {
                MV_ERROR("You have reached the int64_t limit of instructions. Well done\n");
                exit(1);
            }
        }
    }
}

/**
 * @brief Read an instruction from the pipe
 *
 * @param pipe Pipe to read from
 */
mv_instruction_t *mv_read_instruction_from_pipe(mv_pipe *pipe)
{

    // Wait for the lock
    while (pthread_mutex_lock(&pipe->instruction_buffer_lock) != 0)
    {
    }

    if (pipe->instruction_count <= 0)
    {
        pthread_mutex_unlock(&pipe->instruction_buffer_lock);
        return NULL;
    }

    if (pipe->index >= pipe->instruction_count)
    {
        pipe->index = 0;
        pipe->instruction_count = 0;
        pthread_mutex_unlock(&pipe->instruction_buffer_lock);
        return NULL;
    }

    mv_instruction_t *result = &pipe->instruction_buffer[pipe->index++];

    pthread_mutex_unlock(&pipe->instruction_buffer_lock);

    return result;
}