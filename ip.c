#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#include "platform/linux/platform.h"
#include "util.h"
#include "arp.h"
#include "net.h"
#include "ip.h"

struct ip_hdr
{
  uint8_t vhl;       // version(4bit) & IHL(4bit)
  uint8_t tos;       // type of service
  uint16_t total;    // total length
  uint16_t id;       // identification
  uint16_t offset;   // Flags & Fragment Offset
  uint8_t ttl;       // time to live
  uint8_t protocol;  // protocol number
  uint16_t sum;      // checksum
  ip_addr_t src;     // source address
  ip_addr_t dst;     // destination address
  uint8_t options[]; // options
};

// almost same as net_protocol except not has queue
struct ip_protocol
{
  struct ip_protocol *next;
  uint8_t type;
  void (*handler)(const uint8_t *data, size_t len, ip_addr_t src, ip_addr_t dst, struct ip_iface *iface);
};

struct ip_route
{
  struct ip_route *next;
  ip_addr_t network;
  ip_addr_t netmask;
  ip_addr_t nexthop;
  struct ip_iface *iface;
};

const ip_addr_t IP_ADDR_ANY = 0x00000000;       /* 0.0.0.0 */
const ip_addr_t IP_ADDR_BROADCAST = 0xffffffff; /* 255.255.255.255 */

/* NOTE: if you want to add/delete the entries after net_run(), you need to protect these lists with a mutex. */
static struct ip_iface *ifaces;
static struct ip_protocol *protocols;
static struct ip_route *routes;

int ip_addr_pton(const char *p, ip_addr_t *n)
{
  char *sp, *ep;
  int idx;
  long ret;
  sp = (char *)p;
  for (idx = 0; idx < 4; idx++)
  {
    ret = strtol(sp, &ep, 10);
    if (ret < 0 || ret > 255)
    {
      return -1;
    }
    if (ep == sp)
    {
      return -1;
    }
    if ((idx == 3 && *ep != '\0') || (idx != 3 && *ep != '.'))
    {
      return -1;
    }
    ((uint8_t *)n)[idx] = ret;
    sp = ep + 1;
  }
  return 0;
}
char *
ip_addr_ntop(ip_addr_t n, char *p, size_t size)
{
  uint8_t *u8;
  u8 = (uint8_t *)&n;
  snprintf(p, size, "%d.%d.%d.%d", u8[0], u8[1], u8[2], u8[3]);
  return p;
}

int ip_endpoint_pton(const char *p, struct ip_endpoint *n)
{
  char *sep;
  char addr[IP_ADDR_STR_LEN] = {};
  long int port;
  sep = strrchr(p, ':');
  if (!sep)
  {
    return -1;
  }
  memcpy(addr, p, sep - p);
  if (ip_addr_pton(addr, &n->addr) == -1)
  {
    return -1;
  }
  port = strtol(sep + 1, NULL, 10);
  if (port <= 0 || port > UINT16_MAX)
  {
    return -1;
  }
  n->port = hton16(port);
  return 0;
}

char *
ip_endpoint_ntop(const struct ip_endpoint *n, char *p, size_t size)
{
  size_t offset;
  ip_addr_ntop(n->addr, p, size);
  offset = strlen(p);
  snprintf(p + offset, size - offset, ":%d", ntoh16(n->port));
  return p;
}

static void
ip_dump(const uint8_t *data, size_t len)
{
  struct ip_hdr *hdr;
  uint8_t v, hl, hlen;
  uint16_t total, offset;
  char addr[IP_ADDR_STR_LEN];

  flockfile(stderr);
  hdr = (struct ip_hdr *)data;
  v = (hdr->vhl & 0xf0) >> 4; // version
  hl = hdr->vhl & 0x0f;       // IHL
  hlen = hl << 2;
  fprintf(stderr, "        vhl: 0x%02x [v: %u, hl: %u, (%u)]\n", hdr->vhl, v, hl, hlen);
  fprintf(stderr, "        tos: 0x%02x\n", hdr->tos);
  total = ntoh16(hdr->total); // 多バイト長のデータはエンディアンをリトルエンディアンに変換
  fprintf(stderr, "      total: %u (payload: %u)\n", total, total - hlen);
  fprintf(stderr, "         id: %u\n", ntoh16(hdr->id));
  offset = ntoh16(hdr->offset);
  fprintf(stderr, "     offset: 0x%04x [flags=%x, offset=%u]\n", offset, (offset & 0xe000) >> 13, (offset & 0x1fff));
  fprintf(stderr, "        ttl: %u\n", hdr->ttl);
  fprintf(stderr, "   protocol: %u\n", hdr->protocol);
  fprintf(stderr, "        sum: 0x%04x\n", ntoh16(hdr->sum));
  fprintf(stderr, "        src: %s\n", ip_addr_ntop(hdr->src, addr, sizeof(addr)));
  fprintf(stderr, "        dst: %s\n", ip_addr_ntop(hdr->dst, addr, sizeof(addr)));
#ifdef HEXDUMP
  hexdump(stderr, data, len);
#endif
  funlockfile(stderr);
}

/* NOTE: must not be call after net_run() */
static struct ip_route *
ip_route_add(ip_addr_t network, ip_addr_t netmask, ip_addr_t nexthop, struct ip_iface *iface)
{
  struct ip_route *route;
  char addr1[IP_ADDR_STR_LEN];
  char addr2[IP_ADDR_STR_LEN];
  char addr3[IP_ADDR_STR_LEN];
  char addr4[IP_ADDR_STR_LEN];

  route = memory_alloc(sizeof(*route));
  if (!route)
  {
    errorf("memory_alloc() failure");
    return NULL;
  }
  route->network = network;
  route->netmask = netmask;
  route->nexthop = nexthop;
  route->iface = iface;
  route->next = routes;
  routes = route;
  infof("network=%s, netmask=%s, nexthop=%s, iface=%s dev=%s",
        ip_addr_ntop(route->network, addr1, sizeof(addr1)),
        ip_addr_ntop(route->netmask, addr2, sizeof(addr2)),
        ip_addr_ntop(route->nexthop, addr3, sizeof(addr3)),
        ip_addr_ntop(route->iface->unicast, addr4, sizeof(addr4)),
        NET_IFACE(iface)->dev->name);
  return route;
}

static struct ip_route *
ip_route_lookup(ip_addr_t dst)
{
  struct ip_route *route, *candidate = NULL;

  for (route = routes; route; route = route->next)
  {
    if ((dst & route->netmask) == route->network)
    {
      if (!candidate || ntoh32(candidate->netmask) < ntoh32(route->netmask))
      {
        candidate = route;
      }
    }
  }
  return candidate;
}

/* NOTE: must not be call after net_run() */
int ip_route_set_default_gateway(struct ip_iface *iface, const char *gateway)
{
  ip_addr_t gw;

  if (ip_addr_pton(gateway, &gw) == -1)
  {
    errorf("ip_addr_pton() failure, addr=%s", gateway);
    return -1;
  }
  if (!ip_route_add(IP_ADDR_ANY, IP_ADDR_ANY, gw, iface))
  {
    errorf("ip_route_add() failure");
    return -1;
  }
  return 0;
}

struct ip_iface *
ip_route_get_iface(ip_addr_t dst)
{
  struct ip_route *route;

  route = ip_route_lookup(dst);
  if (!route)
  {
    return NULL;
  }
  return route->iface;
}

struct ip_iface *
ip_iface_alloc(const char *unicast, const char *netmask)
{
  struct ip_iface *iface;

  iface = memory_alloc(sizeof(*iface));
  if (!iface)
  {
    errorf("memory_alloc() failure");
    return NULL;
  }
  NET_IFACE(iface)->family = NET_IFACE_FAMILY_IP;

  ip_addr_t uni;
  if (ip_addr_pton(unicast, &uni) == -1)
  {
    memory_free(iface);
    errorf("unicast ip cast failure");
    return NULL;
  }
  iface->unicast = uni;

  ip_addr_t mask;
  if (ip_addr_pton(netmask, &mask) == -1)
  {
    memory_free(iface);
    errorf("netmask cast failure");
    return NULL;
  }
  iface->netmask = mask;

  iface->broadcast = (uni & mask) | (~mask);

  return iface;
}
/* NOTE: must not be call after net_run() */
// register ifaces variable and device's iface
int ip_iface_register(struct net_device *dev, struct ip_iface *iface)
{
  char addr1[IP_ADDR_STR_LEN];
  char addr2[IP_ADDR_STR_LEN];
  char addr3[IP_ADDR_STR_LEN];

  if (net_device_add_iface(dev, iface))
  {
    errorf("add iface failed");
    return -1;
  }
  if (!ip_route_add(iface->unicast & iface->netmask, iface->netmask, IP_ADDR_ANY, iface))
  {
    errorf("ip_route_add() failure");
    return -1;
  }
  iface->next = ifaces;
  ifaces = iface;

  infof("registered: dev=%s, unicast=%s, netmask=%s, broadcast=%s", dev->name,
        ip_addr_ntop(iface->unicast, addr1, sizeof(addr1)),
        ip_addr_ntop(iface->netmask, addr2, sizeof(addr2)),
        ip_addr_ntop(iface->broadcast, addr3, sizeof(addr3)));
  return 0;
}

struct ip_iface *
ip_iface_select(ip_addr_t addr)
{
  struct ip_iface *entry;
  for (entry = ifaces; entry; entry = entry->next)
  {
    if (entry->unicast == addr)
    {
      return entry;
    }
  }
  warnf("not found device which has specified addr");
  return NULL;
}

/* NOTE: must not be call after net_run() */
int ip_protocol_register(uint8_t type, void (*handler)(const uint8_t *data, size_t len, ip_addr_t src, ip_addr_t dst, struct ip_iface *iface))
{
  struct ip_protocol *entry;

  for (entry = protocols; entry != NULL; entry = entry->next)
  {
    if (entry->type == type)
    {
      errorf("%u is already registered", entry->type);
      return -1;
    }
  }

  entry = memory_alloc(sizeof(*entry));
  if (!entry)
  {
    errorf("memory allocation of ip protocol is failed");
    return -1;
  }
  entry->next = protocols;
  entry->handler = handler;
  entry->type = type;

  protocols = entry;

  infof("registered, type=%u", entry->type);
  return 0;
}

// ip input handler, this called when recieve packet from net device
// data is uint8_t data[], data is ip header and payload
// len is net_protocol_queue_entry.len
// dev is device recieved packet
static void
ip_input(const uint8_t data[], size_t len, struct net_device *dev)
{
  struct ip_hdr *hdr;
  uint8_t v;
  uint16_t hlen, total, offset;
  struct ip_iface *iface;
  char addr[IP_ADDR_STR_LEN];

  if (len < IP_HDR_SIZE_MIN)
  {
    errorf("ip header size is too short: %zu", len);
    return;
  }
  hdr = (struct ip_hdr *)data;

  v = (hdr->vhl & 0xf0) >> 4;
  if (v != IP_VERSION_IPV4)
  {
    errorf("version must be 4");
    return;
  }

  hlen = (hdr->vhl & 0x0f) << 2;
  if (len < hlen)
  {
    errorf("header data is too short");
    return;
  }

  total = ntoh16(hdr->total);
  if (len < total)
  {
    errorf("header total is too short");
    return;
  }

  // pass a pointer to the beginning of the header in uint16_t for processing 16 bits at a time
  if (cksum16((uint16_t *)hdr, hlen, 0) != 0)
  {
    errorf("checksum validation failed");
    return;
  }

  offset = ntoh16(hdr->offset);
  if (offset & 0x2000 || offset & 0x1fff)
  {
    errorf("flagments does not support");
    return;
  }

  iface = net_device_get_iface(dev, NET_IFACE_FAMILY_IP);
  if (!iface)
  {
    errorf("coudln't get iface");
    return;
  }
  if ((hdr->dst != iface->unicast) && (hdr->dst != IP_ADDR_BROADCAST) && (hdr->dst != iface->broadcast))
  {
    return;
  }
  debugf("dev=%s, iface=%s, protocol=%u, total=%u", dev->name, ip_addr_ntop(iface->unicast, addr, sizeof(addr)), hdr->protocol, total);
  ip_dump(data, total);

  struct ip_protocol *entry;
  for (entry = protocols; entry; entry = entry->next)
  {
    if (entry->type == hdr->protocol)
    {
      entry->handler((uint8_t *)hdr + hlen, total - hlen, hdr->src, hdr->dst, iface);
      return;
    }
  }

  /* unsupported protocol*/
}

// data is ip header + payload
// len is total length of ip packet
static int
ip_output_device(struct ip_iface *iface, const uint8_t *data, size_t len, ip_addr_t dst)
{
  uint8_t hwaddr[NET_DEVICE_ADDR_LEN];
  int ret;

  if (NET_IFACE(iface)->dev->flags & NET_DEVICE_FLAG_NEED_ARP)
  {
    if (dst == iface->broadcast || dst == IP_ADDR_BROADCAST)
    {
      memcpy(hwaddr, NET_IFACE(iface)->dev->broadcast, NET_IFACE(iface)->dev->alen);
    }
    else
    {
      ret = arp_resolve(NET_IFACE(iface), dst, hwaddr);
      if (ret != ARP_RESOLVE_FOUND)
      {
        debugf("arp not found");
        return ret;
      }
    }
  }
  debugf("ip output device done");
  return net_device_output(NET_IFACE(iface)->dev, NET_PROTOCOL_TYPE_IP, data, len, hwaddr);
}

// protocol is IP(1)
// data is payload(start from offset)
// len is sizeof(data)
// id is identification
// offset is Fragment offset(at first 0)
static ssize_t
ip_output_core(struct ip_iface *iface, uint8_t protocol, const uint8_t *data, size_t len, ip_addr_t src, ip_addr_t dst, ip_addr_t nexthop, uint16_t id, uint16_t offset)
{
  uint8_t buf[IP_TOTAL_SIZE_MAX]; // header + payload
  struct ip_hdr *hdr;
  uint16_t hlen, total;
  char addr[IP_ADDR_STR_LEN];

  hdr = (struct ip_hdr *)buf;

  hlen = IP_HDR_SIZE_MIN;
  hdr->vhl = (IP_VERSION_IPV4 << 4) | (hlen >> 2);
  hdr->tos = 0;
  total = hlen + len; // header + payload

  // only translate multi bytes fields order
  // don't reorder IP Header entirely
  hdr->total = hton16(total);
  hdr->id = hton16(id);
  hdr->offset = hton16(offset);

  hdr->ttl = 0xff;
  hdr->protocol = protocol;
  hdr->sum = 0; // according to RFC791, checksum field itself must be 0 when calc checksum
  hdr->src = src;
  hdr->dst = dst;
  hdr->sum = cksum16((uint16_t *)hdr, hlen, 0);
  memcpy(hdr + 1, data, len); // add payload to right behind header

  debugf("dev=%s, dst=%s, protocol=%u, len=%u",
         NET_IFACE(iface)->dev->name, ip_addr_ntop(dst, addr, sizeof(addr)), protocol, total);
  ip_dump(buf, total);
  // buf's stack memory release when done ip_output_device
  return ip_output_device(iface, buf, total, nexthop);
}

static uint16_t
ip_generate_id(void)
{
  static mutex_t mutex = MUTEX_INITIALIZER;
  static uint16_t id = 128;
  uint16_t ret;
  mutex_lock(&mutex);
  ret = id++;
  mutex_unlock(&mutex);
  return ret;
}

// protocol is IP(1)
// data is payload(start from offset)
// len is sizeof(data)
ssize_t
ip_output(uint8_t protocol, const uint8_t *data, size_t len, ip_addr_t src, ip_addr_t dst)
{
  struct ip_route *route;
  struct ip_iface *iface;
  char addr[IP_ADDR_STR_LEN];
  ip_addr_t nexthop;
  uint16_t id;

  if (src == IP_ADDR_ANY && dst == IP_ADDR_BROADCAST)
  {
    errorf("source address is required for broadcast addresses");
    return -1;
  }
  // get ip route info
  route = ip_route_lookup(dst);
  if (!route)
  {
    errorf("no route to host, addr=%s", ip_addr_ntop(dst, addr, sizeof(addr)));
    return -1;
  }
  iface = route->iface;
  // source IP must be the same as the unicast IP of the interface
  if (src != IP_ADDR_ANY && src != iface->unicast)
  {
    errorf("unable to output with specified source address, addr=%s", ip_addr_ntop(src, addr, sizeof(addr)));
    return -1;
  }
  // nexthop is not equal to dest of ip header
  nexthop = (route->nexthop != IP_ADDR_ANY) ? route->nexthop : dst;
  if (NET_IFACE(iface)->dev->mtu < IP_HDR_SIZE_MIN + len)
  {
    errorf("too long, dev=%s, mtu=%u < %zu",
           NET_IFACE(iface)->dev->name, NET_IFACE(iface)->dev->mtu, IP_HDR_SIZE_MIN + len);
    return -1;
  }
  id = ip_generate_id();
  if (ip_output_core(iface, protocol, data, len, iface->unicast, dst, nexthop, id, 0) == -1)
  {
    errorf("ip_output_core() failed");
    return -1;
  }
  return len;
}

// register protocol(net.c) to ip handler
int ip_init(void)
{
  if (net_protocol_register(NET_PROTOCOL_TYPE_IP, ip_input) == -1)
  {
    errorf("net_protocol_register() failed");
    return -1;
  }
  return 0;
}