#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const {
    return _output_bytes;
}

void TCPSender::fill_window() {
    size_t real_window_size = _remain_window_size;
    if(real_window_size == 0)
        real_window_size = 1;
    
    while(real_window_size > _output_bytes) {
        TCPSegment seg;
        if(!_set_syn_flag) {
            seg.header().syn = true;
            _set_syn_flag = true;
        }
        
        seg.header().seqno = next_seqno();
        size_t payload_length = min(TCPConfig::MAX_PAYLOAD_SIZE, real_window_size - _output_bytes - seg.header().syn);
        string payload = _stream.read(payload_length);
        

        if(!_set_fin_flag && _stream.eof() && _output_bytes + payload.size() < real_window_size) {
            seg.header().fin = true;
            _set_fin_flag = true;
        }
        seg.payload() = Buffer(move(payload));

        if(seg.length_in_sequence_space() == 0) 
            break;
        
        if(_output_segments.empty()) {
            _rto = _initial_retransmission_timeout;
            _now_time = 0;
        }

        _segments_out.push(seg);
        _output_bytes += seg.length_in_sequence_space();
        _output_segments.insert(make_pair(_next_seqno, seg));
        _next_seqno += seg.length_in_sequence_space();

        if(seg.header().fin)
            break;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    uint64_t abs_ack = unwrap(ackno, _isn, _next_seqno);
    if(abs_ack > _next_seqno)
        return;
    auto iter = _output_segments.begin();
    while(iter != _output_segments.end()) {
        TCPSegment &seg = iter->second;
        if(iter->first + seg.length_in_sequence_space() <= abs_ack) {
            _output_bytes -= seg.length_in_sequence_space();
            iter = _output_segments.erase(iter);
            _rto = _initial_retransmission_timeout;
            _now_time = 0;
        }
        else 
            break;
    }
    _retransmit_count = 0;
    _remain_window_size = window_size;
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _now_time += ms_since_last_tick;
    auto iter = _output_segments.begin();
    if(iter == _output_segments.end())
        return;
    if(_now_time >= _rto) {
        if(_remain_window_size != 0)
            _rto *= 2;
        _now_time = 0;
        _segments_out.push(iter->second);
        _retransmit_count++;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { 
    return _retransmit_count;
}

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}
