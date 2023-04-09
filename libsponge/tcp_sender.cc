#include "tcp_sender.hh"

#include "tcp_config.hh"
#include <iostream>
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
    , _stream(capacity)
    , _timer(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _absolute_ack; }

void TCPSender::fill_window() {

    if(stream_in().eof() && next_seqno_absolute() == stream_in().bytes_written() + 2 && bytes_in_flight() == 0){

        return;
    }

    if(_window_size == 0){

        return;
    }

    TCPSegment seg;
    TCPHeader &header = seg.header();
    if(next_seqno_absolute() == 0){

        header.seqno = next_seqno();
        header.syn = true;
        _window_size--;
        _next_seqno++;

        _segments_out.push(seg);
        _outstanding_segments.push(seg);
    }else if(stream_in().eof() && next_seqno_absolute() < stream_in().bytes_written() + 2 && _window_size > 0){

        header.seqno = next_seqno();
        header.fin = true;
        _window_size--;
        _next_seqno++;

        _segments_out.push(seg);
        _outstanding_segments.push(seg);
    }else{

        while(!stream_in().buffer_empty() && _window_size > 0){

            header.seqno = next_seqno();
            uint64_t _send_length = min(TCPConfig::MAX_PAYLOAD_SIZE, min(_window_size, stream_in().buffer_size()));

            seg.payload() = stream_in().read(_send_length);
            _window_size -= _send_length;
            _next_seqno += _send_length;
            if(stream_in().eof() && next_seqno_absolute() < stream_in().bytes_written() + 2 && _window_size > 0){

                header.fin = true;
                _window_size--;
                _next_seqno++;
            }

            _segments_out.push(seg);
            _outstanding_segments.push(seg);
        }
    }

    if(!_timer.is_running()){

        _timer.set_running();
        _timer.set_initial(_initial_retransmission_timeout);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 

    uint64_t _current_ack = unwrap(ackno, _isn, _checkpoint);
    
    if(_current_ack > _next_seqno || _current_ack < _absolute_ack){

        return;
    }
    
    if(window_size == 0) {
        _windows_zero_flag = true;
        _window_size = 1;
    } else {
        _windows_zero_flag = false;
        _window_size = window_size;
    }

    TCPSegment seg;
    uint64_t temp;

    while(!_outstanding_segments.empty()){

        seg = _outstanding_segments.front();
        temp = unwrap(seg.header().seqno, _isn, _checkpoint) + seg.length_in_sequence_space();
        if(temp > _current_ack){

            break;
        }else{
            
            _absolute_ack = temp;
            _outstanding_segments.pop();
            _timer.set_initial(_initial_retransmission_timeout);
            _timer.set_running();
        }
    }

    if (bytes_in_flight() > _window_size) {

        _window_size = 0;
        _windows_zero_flag = true;
        return;
    }
    if(bytes_in_flight() == 0){

        _timer.set_stopping();
    }
    
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {

    if(!_timer.is_running()){

        return;
    }

    if(_timer.check_eclipese(ms_since_last_tick) && !_outstanding_segments.empty()){

        TCPSegment rolling = _outstanding_segments.front();
        _segments_out.push(rolling);
        
        if(!_windows_zero_flag){

            _timer.add_retransmissions_time();
            _timer.double_time();
        }

        _timer.set_zero();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _timer.get_retransmissions_time(); }

void TCPSender::send_empty_segment() {

    TCPSegment seg;
    TCPHeader &header = seg.header();
    header.seqno = next_seqno();
    _segments_out.push(seg);
}