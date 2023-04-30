#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {

    _time_since_last_segment_received = 0;

    // all error
    if (seg.header().rst) {
        set_errorr();
        return;
    }

    _receiver.segment_received(seg);

    // normal trans
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }

    if (seg.header().syn) {

        _sender.fill_window();

        // if it's not the above case, send a plain empty segment
        if (_sender.segments_out().size() == 0){
            _sender.send_empty_segment(false);
            _sender.setInitialTimer();
        }
        send_final_seg();
        return;
    }


    if (seg.length_in_sequence_space() > 0 && _sender.segments_out().size() == 0) {

        _sender.send_empty_segment(false);
        _sender.setInitialTimer();
    }
    if (_receiver.ackno().has_value() &&
        seg.length_in_sequence_space() == 0 &&
        seg.header().seqno == _receiver.ackno().value() - 1) {

        _sender.send_empty_segment(false);
        _sender.setInitialTimer();
    }

    send_final_seg();

    if (_receiver.stream_out().input_ended() && !_sender.stream_in().input_ended()) {
        _linger_after_streams_finish = false;
    }
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    size_t written_len = _sender.stream_in().write(data);

    _sender.fill_window();
    send_final_seg();
    return written_len;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {

    if (_sender.consecutive_retransmissions() >= TCPConfig::MAX_RETX_ATTEMPTS) {
        send_errorr();
        send_final_seg();
        return;
    }

    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (try_prereqs()){

        if(_linger_after_streams_finish){

                if(_time_since_last_segment_received >= 10 * _cfg.rt_timeout){
                    
                    clean_shut_down();
                }
        }else{

            clean_shut_down();
        }
    }

    send_final_seg();
} 

void TCPConnection::end_input_stream() {
    // establised to fin wait
    _sender.stream_in().end_input();
    _sender.fill_window();
    _sender.setInitialTimer();
    send_final_seg();
}

void TCPConnection::connect() {
    // closed to syn_sent
    _sender.fill_window();
    _sender.setInitialTimer();
    send_final_seg();
}

void TCPConnection::set_errorr() {
    _active = false;
    _linger_after_streams_finish = true;

    while (!_sender.segments_out().empty()) {
        _sender.segments_out().pop();
    }
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
}

void TCPConnection::send_errorr() {
    set_errorr();
    _sender.send_empty_segment(true);
}

bool TCPConnection::try_prereqs() {
    bool prereq1 = unassembled_bytes() == 0 && _receiver.stream_out().input_ended();
    bool prereq2 =
        _sender.stream_in().input_ended() && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2;
    bool prereq3 = bytes_in_flight() == 0;
    return prereq1 && prereq2 && prereq3;
}
void TCPConnection::clean_shut_down() {
    _linger_after_streams_finish = true;
    _active = false;
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            send_errorr();
            send_final_seg();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_final_seg() {
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        TCPHeader &header = seg.header();
        _sender.segments_out().pop();

        if (_receiver.ackno().has_value()) {
            header.ack = true;
            header.ackno = _receiver.ackno().value();
            header.win = _receiver.window_size();
        }

        _segments_out.push(seg);
    }
}