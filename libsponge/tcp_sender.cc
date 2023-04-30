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

    size_t remaining_winsize = (_window_size != 0 ? _window_size : 1);

    size_t out_size = bytes_in_flight();
    if (remaining_winsize < out_size)
        return;
        
    remaining_winsize -= out_size;

    if(stream_in().eof() && next_seqno_absolute() == stream_in().bytes_written() + 2 && bytes_in_flight() == 0){

        return;
    }

    if(next_seqno_absolute() == 0 && !_syn){

        TCPSegment seg;
        TCPHeader &header = seg.header();
        header.seqno = next_seqno();
        header.syn = true;
        remaining_winsize--;
        _next_seqno++; 
        _syn = true;

        _segments_out.push(seg);
        _outstanding_segments.push(seg);
    }else if(stream_in().eof() && next_seqno_absolute() < stream_in().bytes_written() + 2 && remaining_winsize > 0 && !_fin){

        TCPSegment seg;
        TCPHeader &header = seg.header();
        header.seqno = next_seqno();
        header.fin = true;
        remaining_winsize--;
        _next_seqno++;
        _fin = true;
        _segments_out.push(seg);
        _outstanding_segments.push(seg);
    }else{

        while(true){

            TCPSegment seg;
            TCPHeader &header = seg.header();
            size_t seg_size = remaining_winsize;
            if(seg_size == 0){

                break;
            }
            
            header.seqno = next_seqno();
            string seg_data = _stream.read(min(seg_size, TCPConfig::MAX_PAYLOAD_SIZE));
            seg_size -= seg_data.size();
            seg.payload() = Buffer(move(seg_data));
            if(!_fin && stream_in().eof() && seg_size > 0){

                seg_size--;
                header.fin = true;
                _fin = true;
            }

            seg_size = seg.length_in_sequence_space();
            // if the segment's actual size is 0, it shouldn't been sent
            if (seg_size == 0)
                break;

            remaining_winsize -= seg_size;
            _next_seqno += seg_size;

            _segments_out.push(seg);
            _outstanding_segments.push(seg);

            if(!_timer.is_running()){

                _timer.set_running();
                _timer.set_initial(_initial_retransmission_timeout);
            }
        }

        return;
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
    
    if(_current_ack > _next_seqno){

        return;
    }
    
    _window_size = window_size;

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

    if(_timer.check_eclipese(ms_since_last_tick)){

        if(!_outstanding_segments.empty()){
            TCPSegment rolling = _outstanding_segments.front();
            _segments_out.push(rolling);
        }
        
        if(_window_size > 0){

            _timer.add_retransmissions_time();
            _timer.double_time();
        }

        _timer.set_zero();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _timer.get_retransmissions_time(); }

void TCPSender::send_empty_segment(bool rst) {

    TCPSegment seg;
    TCPHeader &header = seg.header();
    header.seqno = next_seqno();
    if(rst){

        header.rst = true;
    }
    _segments_out.push(seg);
}