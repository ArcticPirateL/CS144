#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if(!_set_syn_flag) {
        if(!seg.header().syn)                   
            return;
        _set_syn_flag = true;                   
        _isn = seg.header().seqno.raw_value();
    }
    else if(seg.header().syn) {                 
        return;
    }
    if(_set_syn_flag && seg.header().fin) {     
        _set_fin_flag = true;
    }
    uint64_t last_segment_absseqno = _reassembler.stream_out().bytes_written() + 1;
    uint64_t this_abs_64 = unwrap(seg.header().seqno, WrappingInt32(_isn), last_segment_absseqno);
    uint64_t stream_index;
    if(seg.header().syn)
        stream_index = 0;                 
    else  
        stream_index = this_abs_64 - 1;   
    _reassembler.push_substring(seg.payload().copy(), stream_index, seg.header().fin);
}


optional<WrappingInt32> TCPReceiver::ackno() const {
    if(!_set_syn_flag)                              
        return nullopt;
    if(_set_fin_flag && _reassembler.stream_out().input_ended()) {   
        return wrap(_reassembler.stream_out().bytes_written() + 2, WrappingInt32(_isn));
    }
    else {
        return wrap(_reassembler.stream_out().bytes_written() + 1, WrappingInt32(_isn));
    }
}

size_t TCPReceiver::window_size() const {
    return _capacity - _reassembler.stream_out().buffer_size();
}
