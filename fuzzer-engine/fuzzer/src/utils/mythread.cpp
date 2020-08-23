#include <fuzzer/utils/thread.h>
#include <string.h>
void thread_exit_handler(int sig) {
  // printf("this signal is %d \n", sig);
  pthread_exit(0);
}
void my_thread_init() {
  struct sigaction actions;
  memset(&actions, 0, sizeof(actions));
  sigemptyset(&actions.sa_mask);
  actions.sa_flags = 0;
  actions.sa_handler = thread_exit_handler;
  sigaction(SIGUSR1, &actions, NULL);
}