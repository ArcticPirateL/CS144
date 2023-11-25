#ifndef SPONGE_LIBSPONGE_NETWORK_INTERFACE_HH
#define SPONGE_LIBSPONGE_NETWORK_INTERFACE_HH

#include "ethernet_frame.hh"
#include "tcp_over_ip.hh"
#include "tun.hh"


#include <queue>
#include <map>
#include <list>
#include <optional>

//! \brief A "network interface" that connects IP (the internet layer, or network layer)
//! with Ethernet (the network access layer, or link layer).

//! This module is the lowest layer of a TCP/IP stack
//! (connecting IP with the lower-layer network protocol,
//! e.g. Ethernet). But the same module is also used repeatedly
//! as part of a router: a router generally has many network
//! interfaces, and the router's job is to route Internet datagrams
//! between the different interfaces.

//! The network interface translates datagrams (coming from the
//! "customer," e.g. a TCP/IP stack or router) into Ethernet
//! frames. To fill in the Ethernet destination address, it looks up
//! the Ethernet address of the next IP hop of each datagram, making
//! requests with the [Address Resolution Protocol](\ref rfc::rfc826).
//! In the opposite direction, the network interface accepts Ethernet
//! frames, checks if they are intended for it, and if so, processes
//! the the payload depending on its type. If it's an IPv4 datagram,
//! the network interface passes it up the stack. If it's an ARP
//! request or reply, the network interface processes the frame
//! and learns or replies as necessary.
class NetworkInterface {
  private:
    //! Ethernet (known as hardware, network-access-layer, or link-layer) address of the interface
    EthernetAddress _ethernet_address;

    //! IP (known as internet-layer or network-layer) address of the interface
    Address _ip_address;

    //! outbound queue of Ethernet frames that the NetworkInterface wants sent
    std::queue<EthernetFrame> _frames_out{};

    //ARP表，用于存放IP地址和MAC地址的映射关系，以及ttl。
    class EthernetAddress_to_TTL {
      public:
        EthernetAddress _ethernet_addr_arp;
        size_t _ttl;
      public:
        EthernetAddress_to_TTL(EthernetAddress ethernet_addr_arp, size_t ttl) : _ethernet_addr_arp(ethernet_addr_arp),_ttl(ttl) {}
    };
    std::map<uint32_t, EthernetAddress_to_TTL> _arp_table{};
    //存放等待arp应答的待处理IP报文
    class Waiting_ARP_IP {
      public:
        Address _ip_addr;
        InternetDatagram _ip_datagram;
      public:
        Waiting_ARP_IP(Address ip_addr, InternetDatagram ip_datagram) : _ip_addr(ip_addr), _ip_datagram(ip_datagram) {}
    };
    std::list<Waiting_ARP_IP> _ip_waiting_list{};
    //记录已经发送的arp报文中的IP和其TTL，用于处理超时重发
    std::map<uint32_t, size_t> _sent_arp_table{};
    //存储ttl的数值，arp表中缓存时间30s，已经发送的arp表中缓存时间5s
    const size_t _arp_table_ttl = 30 * 1000;
    const size_t _sent_arp_tabel_ttl = 5 * 1000;

  public:
    //! \brief Construct a network interface with given Ethernet (network-access-layer) and IP (internet-layer) addresses
    NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address);

    //! \brief Access queue of Ethernet frames awaiting transmission
    std::queue<EthernetFrame> &frames_out() { return _frames_out; }

    //! \brief Sends an IPv4 datagram, encapsulated in an Ethernet frame (if it knows the Ethernet destination address).

    //! Will need to use [ARP](\ref rfc::rfc826) to look up the Ethernet destination address for the next hop
    //! ("Sending" is accomplished by pushing the frame onto the frames_out queue.)
    void send_datagram(const InternetDatagram &dgram, const Address &next_hop);

    //! \brief Receives an Ethernet frame and responds appropriately.

    //! If type is IPv4, returns the datagram.
    //! If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
    //! If type is ARP reply, learn a mapping from the "sender" fields.
    std::optional<InternetDatagram> recv_frame(const EthernetFrame &frame);
    
    //! \brief Called periodically when time elapses
    void tick(const size_t ms_since_last_tick);
};

#endif  // SPONGE_LIBSPONGE_NETWORK_INTERFACE_HH
