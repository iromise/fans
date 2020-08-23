#ifndef FUZZER_THREAD_H
#define FUZZER_THREAD_H
#include <signal.h>
#include <thread>
#include <unistd.h>
extern void thread_exit_handler(int sig);
extern void my_thread_init();

#endif // FUZZER_THREAD_H