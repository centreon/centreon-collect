#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <ctime>

#define CHECK_INTERVAL 5
#define AVG_STATE_DURATION (7 * 24 * 60)

int main(int argc, char* argv[]) {
  // Initialize random number generator.
  srandom(getpid() + time(NULL));

  // Get last state.
  int last_state(0);
  if (argc >= 2)
    last_state = strtol(argv[1], NULL, 0);

  // Compute next state.
  int next_state(last_state);
  if (!(random() % (AVG_STATE_DURATION / CHECK_INTERVAL)))
    while (next_state == last_state)
      next_state = random() % 4;

  // Output.
  std::cout << "Benchmark plugin|my_first_metric="
    << random() << "%;80;90;0;100"
    << " my_second_metric=" << random() << "B;100000000000;120000000000;0;130000000000"
    << " my_third_metric=" << random() << std::endl;
  return (next_state);
}
