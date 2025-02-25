#include <signal.h>
#include <unistd.h>

#include "net.h"
#include "util.h"
#include "test.h"
#include "driver/dummy.h"

static volatile sig_atomic_t terminate;

static void
on_signal(int s)
{
  (void)s;
  terminate = 1;
}

int main(int argc, char *argv[])
{
  struct net_device *dev;

  signal(SIGINT, on_signal);
  if (net_init() == -1)
  {
    errorf("net_init() failed");
    return -1;
  }
  dev = dummy_init();
  if (!dev)
  {
    errorf("dummy_init() failed");
    return -1;
  }
  if (net_run() == -1)
  {
    errorf("net_run failed");
    return -1;
  }
  while (!terminate)
  {
    if (net_device_output(dev, 0x0800, test_data, sizeof(test_data), NULL) == -1)
    {
      errorf("net_device_output() failed");
      break;
    }
    sleep(1);
  }
}