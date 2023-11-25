#include "wrapping_integers.hh"
#include <cmath>

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    return WrappingInt32{static_cast<uint32_t>(n + isn.raw_value())};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    WrappingInt32 wrap_checkpoint = wrap(checkpoint, isn);
    int64_t x = n - wrap_checkpoint;
    int64_t t = pow(2, 32);
    if(x  + static_cast<int64_t>(pow(2, 31)) < 0 || x + static_cast<int64_t>(checkpoint) < 0) {
        return x + checkpoint + t;
    }
    else if(x + static_cast<int64_t>(checkpoint) - static_cast<int64_t>(pow(2, 31)) > 0 && x > static_cast<int64_t>(pow(2, 31))) {
        return x + checkpoint - t;
    }
    else {
        return x + checkpoint;
    }
}