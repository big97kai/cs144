#include "tcp_receiver.hh"

#include <iostream>
// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {

    if (!_syn) {
        if (!seg.header().syn) {
            return;
        }
        _syn = true;
        _checkpoint = stream_out().bytes_written();
        _isn = seg.header().seqno;
    }

    if (seg.header().fin) {
        _fin = true;
    }
    uint64_t _index;
    _sequnce = unwrap(seg.header().seqno, _isn, _checkpoint);

    _index = _sequnce;

    if(_syn && !seg.header().syn){

        _index--;
    }

    _reassembler.push_substring(seg.payload().copy(), _index, seg.header().fin);

    _ack = stream_out().bytes_written() + 1;
    if(_fin && _reassembler.unassembled_bytes() == 0){
        _ack++;
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_syn){
        return nullopt;
    }

    return wrap(_ack, _isn);
}

size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }
