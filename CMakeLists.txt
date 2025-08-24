cmake_minimum_required(VERSION 3.25)
project(prosim C)

set(CMAKE_C_STANDARD 11)

include_directories(prosim)
set(THREADS_PREFER_PTHREAD_FLAG ON)

add_executable(prosim
        prosim/context.c
        prosim/context.h
        prosim/main.c
        prosim/prio_q.c
        prosim/prio_q.h
        prosim/process.c
        prosim/process.h
        prosim/barrier.h
        prosim/barrier.c

        prosim/message.c
        prosim/message.h)





find_package(Threads REQUIRED)
target_link_libraries(prosim PRIVATE Threads::Threads)