# Channels in C: Concurrent Programming Project

## Overview

This project is a C language implementation of a channel-based synchronization mechanism, enabling communication between threads through message passing. Channels facilitate the development of concurrent programs by allowing multiple senders and receivers to interact with the same channel simultaneously, supporting both blocking and non-blocking operations.

## Key Features

- **Blocking and Non-Blocking Operations**: Supports both blocking and non-blocking modes for sending and receiving messages, allowing for flexible thread synchronization.
- **Buffered Channels Only**: Focus on implementing buffered channels.
- **Concurrency-Friendly**: Designed to handle concurrent read and write operations without compromising data integrity.

## Project Structure

The implementation revolves around several key files:

- `channel.c` and `channel.h`: Core implementation of the channel functionality.
- `buffer.c` and `buffer.h`: Helper constructs for managing a channel's buffer. These are not thread-safe and should not be modified.
- Optional `linked_list.c` and `linked_list.h`: Provided to support additional data structure management but not required for the main implementation.

## Implementation Functions

- `chan_t* channel_create(size_t size)`: Initialize a new channel with a specified buffer size.
- `enum chan_status channel_send(chan_t* channel, void* data, bool blocking)`: Send data over a channel, optionally in blocking mode.
- `enum chan_status channel_receive(chan_t* channel, void** data, bool blocking)`: Receive data from a channel, optionally in blocking mode.
- Additional functions for channel closure, destruction, and select operations are provided.

## Testing and Evaluation

Ensure your implementation passes all provided tests by running `make test`. Use Valgrind and sanitizer tools to detect and correct any memory leaks or data races.


