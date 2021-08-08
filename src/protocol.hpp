#pragma once

namespace xrun {
/*
    Packet xrun to xserver

    # header
        size_t: packet size

    # chunk
        ClientChunkType: chunk type
        (for COMMAND)
            null-terminated string: cwd
            null-terminated string: command
        (for ARGUMENT)
            null-terminated string: argument

    # chunk...
 */
enum ClientChunkType {
    COMMAND,
    ARGUMENT,
};

/*
    Packet between xserver and xclient

    # xclient returns 4 bytes number of workers after receiving WorkerGroupMessage::WORKERS

    # job packet
        size_t: packet length
        null-terminated string: cwd
        null-terminated string: command

    # error packet
        size_t: command length
        byte-array: command
        1 byte: exitted
        (for exitted == 1)
            1 byte: exit code
        (for exitted == 0)
            1 byte: signal number
        size_t: stdout length
        byte-array: stdout
        size_t: stderr length
        byte-array: stderr
 */
enum class WorkerGroupMessage {
    WORKERS, // s <-  c : none :
    DONE,    // s <-  c : : none
    ERROR,   // s <-  c : : error packet
    JOB,     // s  -> c : job packet :
};
} // namespace xrun
