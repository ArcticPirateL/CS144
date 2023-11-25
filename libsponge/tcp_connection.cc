#include "tcp_connection.hh"

#include <iostream>
#include <cassert>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const {
    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const {
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const {
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const {
    return _time_counter;
}

void TCPConnection::segment_received(const TCPSegment &seg) { 
    
    bool need_send_ack = seg.length_in_sequence_space();
    // you code here.
    //你需要考虑到ACK包、RST包等多种情况
    _time_counter = 0;
    _receiver.segment_received(seg);
    if(seg.header().rst) {
        rst_instructions();
        return;
    }
    // if(_sender.segments_out().empty())
    //     return;
    assert(_sender.segments_out().empty());
    if(seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
        //trans_segments();
        if(need_send_ack && !_sender.segments_out().empty()) {
            need_send_ack = false;
        }
        trans_segments();
    }
    //状态变化(按照个人的情况可进行修改)
    // 如果是 LISEN 到了 SYN
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED) {
        // 此时肯定是第一次调用 fill_window，因此会发送 SYN + ACK
        connect();
        return;
    }

    // 判断 TCP 断开连接时是否时需要等待
    // CLOSE_WAIT
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED)
        _linger_after_streams_finish = false;

    // 如果到了准备断开连接的时候。服务器端先断
    // CLOSED
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && !_linger_after_streams_finish) {
        _is_active = false;
        return;
    }

    // 如果收到的数据包里没有任何数据，则这个数据包可能只是为了 keep-alive
    if (need_send_ack) {
        _sender.send_empty_segment();
        trans_segments();
    }
}

bool TCPConnection::active() const {
    return _is_active;
}

size_t TCPConnection::write(const string &data) {
    size_t write_size = _sender.stream_in().write(data);
    _sender.fill_window();
    trans_segments();
    return write_size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    assert(_sender.segments_out().empty());
    _sender.tick(ms_since_last_tick);
    if(_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
        _sender.segments_out().pop();
        TCPSegment seg;
        seg.header().rst = true;
        _segments_out.push(seg);
        rst_instructions();
        return;
    }
    trans_segments();
    _time_counter += ms_since_last_tick;
    if(TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV && 
       TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED    &&
       _linger_after_streams_finish  &&
       _time_counter >= 10 * _cfg.rt_timeout) {
        _is_active = false;
        _linger_after_streams_finish = false;
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    trans_segments();
}

void TCPConnection::connect() {
    _sender.fill_window();
    _is_active = true;
    trans_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            // TCPSegment seg;
            // seg.header().rst = true;
            // _segments_out.push(seg);
            rst_instructions();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
void TCPConnection::rst_instructions() {
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _linger_after_streams_finish = false;
    _is_active = false;
}
void TCPConnection::trans_segments() {
    //TCPSegment temp_seg;
    while(!_sender.segments_out().empty()) {
        TCPSegment temp_seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if(_receiver.ackno().has_value()) {
            temp_seg.header().ack   = true;
            temp_seg.header().ackno = _receiver.ackno().value();
            temp_seg.header().win   = _receiver.window_size();
        }
        _segments_out.push(temp_seg);
    }
}
