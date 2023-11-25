#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    if(_arp_table.find(next_hop_ip) != _arp_table.end()) {        //若目标以太网地址已知，立即发送
        EthernetFrame eth_frame;
        eth_frame.header() = {_arp_table.find(next_hop_ip)->second._ethernet_addr_arp, _ethernet_address, EthernetHeader::TYPE_IPv4};
        eth_frame.payload() = dgram.serialize();
        _frames_out.push(eth_frame);
    }
    else {                                                        //若目标以太网地址未知
        if(_sent_arp_table.find(next_hop_ip) == _sent_arp_table.end() ) {  //若未发送过ARP请求，发送ARP请求
            ARPMessage arp_frame;
            arp_frame.opcode = ARPMessage::OPCODE_REQUEST;
            arp_frame.sender_ethernet_address = _ethernet_address;
            arp_frame.sender_ip_address = _ip_address.ipv4_numeric();
            arp_frame.target_ethernet_address = {};
            arp_frame.target_ip_address = next_hop_ip;

            EthernetFrame eth_frame;
            eth_frame.header() = {ETHERNET_BROADCAST, _ethernet_address, EthernetHeader::TYPE_ARP};
            eth_frame.payload() = arp_frame.serialize();
            _frames_out.push(eth_frame);

            _sent_arp_table.insert(std::pair<uint32_t, size_t>(next_hop_ip, _sent_arp_tabel_ttl));//加入已经发送的ARP队列
        }
        Waiting_ARP_IP waiting(next_hop, dgram);
        _ip_waiting_list.push_back(waiting);                    //加入等待处理的IP报文队列
    }
}

//! \param[in] frame the incoming Ethernet frame
std::optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if(frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST)  //若不是发给自己的，直接忽略
        return nullopt;
    if(frame.header().type == EthernetHeader::TYPE_IPv4) {             //若是IPv4，返回解析后的数据包
        InternetDatagram datagram;
        if(datagram.parse(frame.payload()) == ParseResult::NoError)
            return datagram;
        else 
            return nullopt;
    }
    else if(frame.header().type == EthernetHeader::TYPE_ARP) {        //若是ARP
        ARPMessage arp_frame;
        if(arp_frame.parse(frame.payload()) != ParseResult::NoError)  //解析ARP
            return nullopt;
        EthernetAddress_to_TTL entry(arp_frame.sender_ethernet_address, _arp_table_ttl);
        _arp_table.insert(std::pair<uint32_t, EthernetAddress_to_TTL>(arp_frame.sender_ip_address, entry));   //更新arp表
        for(auto iter = _ip_waiting_list.begin(); iter != _ip_waiting_list.end(); ) {                         //重新发送等待ARP的IP报文
            if(iter->_ip_addr.ipv4_numeric() == arp_frame.sender_ip_address) {
                send_datagram(iter->_ip_datagram, iter->_ip_addr);
                iter = _ip_waiting_list.erase(iter);
            }
            else 
                iter++;
        }
        _sent_arp_table.erase(arp_frame.sender_ip_address);                                                   //删除已发送的ARP记录
        if(arp_frame.opcode == ARPMessage::OPCODE_REQUEST && arp_frame.target_ip_address == _ip_address.ipv4_numeric()) {  //若是发送给自己的ARP请求包，发送ARP响应包
            ARPMessage arp_reply;
            arp_reply.opcode = ARPMessage::OPCODE_REPLY;
            arp_reply.sender_ethernet_address = _ethernet_address;
            arp_reply.sender_ip_address = _ip_address.ipv4_numeric();
            arp_reply.target_ethernet_address = arp_frame.sender_ethernet_address;
            arp_reply.target_ip_address = arp_frame.sender_ip_address;

            EthernetFrame eth_frame;
            eth_frame.header() = {arp_frame.sender_ethernet_address, _ethernet_address, EthernetHeader::TYPE_ARP};
            eth_frame.payload() = arp_reply.serialize();
            _frames_out.push(eth_frame);
        }
        return nullopt;
    }
    else 
        return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { 
    for(auto iter = _arp_table.begin(); iter != _arp_table.end(); ) {     //更新ARP表
        if(iter->second._ttl > ms_since_last_tick) {
            iter->second._ttl -= ms_since_last_tick;
            iter++;
        }
        else 
            iter = _arp_table.erase(iter);
    } 
    for(auto &iter : _sent_arp_table) {
        if(iter.second > ms_since_last_tick) 
            iter.second -= ms_since_last_tick;
        else {
            ARPMessage arp_again;
            arp_again.opcode = ARPMessage::OPCODE_REQUEST;
            arp_again.sender_ethernet_address = _ethernet_address;
            arp_again.sender_ip_address = _ip_address.ipv4_numeric();
            arp_again.target_ethernet_address = {};
            arp_again.target_ip_address = iter.first;

            EthernetFrame eth_frame;
            eth_frame.header() = {ETHERNET_BROADCAST, _ethernet_address, EthernetHeader::TYPE_ARP};
            eth_frame.payload() = arp_again.serialize();
            _frames_out.push(eth_frame);

            iter.second = _sent_arp_tabel_ttl;
        }
    }
}
