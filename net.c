#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "net.h"
#include "ip.h"
#include "util.h"
#include "platform/linux/platform.h"

// represents Layer3 protocols which be needed to be handled by NIC
// this has handler, input queue(buffer), protocol type
struct net_protocol
{
  struct net_protocol *next; // next protocol
  uint16_t type;
  struct queue_head queue; /* input queue */
  void (*handler)(const uint8_t *data, size_t len, struct net_device *dev);
};

// data which included metadata pushed protocol's input queue
struct net_protocol_queue_entry
{
  struct net_device *dev;
  size_t len;
  uint8_t data[];
};

static struct net_device *devices;     // list of devices to be controlled
static struct net_protocol *protocols; // list of protocols to be controlled

// allocate net device memory
struct net_device *
net_device_alloc(void)
{
  struct net_device *dev;
  dev = memory_alloc(sizeof(*dev));

  if (!dev)
  {
    errorf("memory_alloc() failed");
    return NULL;
  }
  return dev;
}

// register net device to variable "devices"
int net_device_register(struct net_device *dev)
{
  static unsigned int index = 0;

  dev->index = index++;
  snprintf(dev->name, sizeof(dev->name), "net%d", dev->index);
  dev->next = devices;
  devices = dev;
  infof("registered, dev=%s, type=0x%04x", dev->name, dev->type);
  return 0;
}

// call net_device->ops->open(dev) to open specified device
static int
net_device_open(struct net_device *dev)
{
  if (NET_DEVICE_IS_UP(dev))
  {
    errorf("already opened, dev=%s", dev->name);
    return -1;
  }
  if (dev->ops->open)
  {
    if (dev->ops->open(dev) == -1)
    {
      errorf("falied, dev=%s", dev->name);
      return -1;
    }
  }
  dev->flags |= NET_DEVICE_FLAG_UP;
  infof("dev=%s, state=%s", dev->name, NET_DEVICE_STATE(dev));
  return 0;
}

// call net_device->ops->close(dev) to close specified device
static int
net_device_close(struct net_device *dev)
{
  if (!NET_DEVICE_IS_UP(dev))
  {
    errorf("not opened, dev=%s", dev->name);
    return -1;
  }
  if (dev->ops->close)
  {
    if (dev->ops->close(dev) == -1)
    {
      errorf("falied, dev=%s", dev->name);
      return -1;
    }
  }
  dev->flags &= ~NET_DEVICE_FLAG_UP;
  infof("dev=%s, state=%s", dev->name, NET_DEVICE_STATE(dev));
  return 0;
}

// call net_device->ops->transmit(...) to transmit data to specified device
int net_device_output(struct net_device *dev, uint16_t type, const uint8_t *data, size_t len, const void *dst)
{
  if (!NET_DEVICE_IS_UP(dev))
  {
    errorf("not opened, dev=%s", dev->name);
    return -1;
  }

  if (len > dev->mtu)
  {
    errorf("too long, dev=%s mtu=%u, len=%zu", dev->name, dev->mtu, len);
  }
  debugf("dev=%s, type=0x%04x, len=%zu", dev->name, type, len);
  debugdump(data, len);
  if (dev->ops->transmit(dev, type, data, len, dst) == -1)
  {
    errorf("device transmit failed, dev=%s, len=%zu", dev->name, len);
    return -1;
  }
  return 0;
}

/* NOTE: must not be call after net_run() */
int net_protocol_register(uint16_t type, void (*handler)(const uint8_t *data, size_t len, struct net_device *dev))
{
  struct net_protocol *proto;

  for (proto = protocols; proto; proto = proto->next)
  {
    if (type == proto->type)
    {
      errorf("already registered, type=0x%04x", type);
      return -1;
    }
  }
  proto = memory_alloc(sizeof(*proto));
  if (!proto)
  {
    errorf("memory_alloc() failed");
    return -1;
  }
  proto->type = type;
  proto->handler = handler;
  proto->next = protocols;
  protocols = proto;
  infof("registered, type=0x%04x", type);
  return 0;
}

// handler which called when net device recieved packet and interruptted by signal(imitate hardware interruption)
int net_input_handler(uint16_t type, const uint8_t *data, size_t len, struct net_device *dev)
{
  struct net_protocol *proto;
  struct net_protocol_queue_entry *entry;

  for (proto = protocols; proto; proto = proto->next)
  {
    if (proto->type == type)
    {
      // need to allocate struct + variable-length data size
      entry = memory_alloc(sizeof(*entry) + len);
      if (!entry)
      {
        errorf("memory_alloc() failed");
        return -1;
      }

      entry->dev = dev;
      entry->len = len;
      memcpy(entry->data, data, len);

      if (!queue_push(&proto->queue, entry))
      {
        errorf("queue_push() failed");
        memory_free(entry);
        return -1;
      }

      debugf("protocol queue pushed ()num:%u. dev=%s, type=0x%04x, len=%zu",
             proto->queue.num, dev->name, type, len);
      debugdump(data, len);
      intr_raise_irq(INTR_IRQ_SOFTIRQ);
      return 0;
    }
  }
  /* unsupported protocl */
  return 0;
}

// handler which called when recieved software irq
int net_softirq_handler(void)
{
  struct net_protocol *proto;
  struct net_protocol_queue_entry *entry;

  for (proto = protocols; proto; proto = proto->next)
  {
    while (1)
    {
      entry = queue_pop(&proto->queue);
      if (!entry)
      {
        break;
      }
      debugf("protocol queue popped (num:%u), dev=%s, type=0x%04x len=%zu", proto->queue.num, entry->dev->name, proto->type, entry->len);
      debugdump(entry->data, entry->len);
      proto->handler(entry->data, entry->len, entry->dev);
      memory_free(entry);
    }
  }
  return 0;
}

int net_run(void)
{
  struct net_device *dev;

  if (intr_run() == -1)
  {
    errorf("intr_run() failed");
    return -1;
  }

  debugf("open all devices...");
  for (dev = devices; dev; dev = dev->next)
  {
    net_device_open(dev);
  }
  debugf("running...");
  return 0;
}

void net_shutdown(void)
{
  struct net_device *dev;

  debugf("close all devices...");
  for (dev = devices; dev; dev = dev->next)
  {
    net_device_close(dev);
  }
  intr_shutdown();
  debugf("shutting down");
}

// initialize entirely network device
int net_init(void)
{
  if (intr_init() == -1)
  {
    errorf("intr_init() failed");
    return -1;
  }
  if (ip_init() == -1)
  {
    errorf("ip_init() failed");
    return -1;
  }
  infof("initialized");
  return 0;
}
