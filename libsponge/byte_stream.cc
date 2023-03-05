#include "byte_stream.hh"
#include <iostream>
// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

size_t ByteStream::write(const string &data) {
    size_t length = data.length();

    if (length > remaining_capacity()) {
        length = remaining_capacity();
    }
    writeCharNumber += length;

    for (size_t i = 0; i < length; i++) {
        buffer.push_back(data[i]);
    }
    return length;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t length = len;

    if (length > buffer_size()) {
        length = buffer_size();
    }
    return string().assign(buffer.begin(), buffer.begin() + length);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t length = len;

    if (length > buffer_size()) {
        length = buffer_size();
    }

    for (size_t i = 0; i < length; i++) {
        buffer.pop_front();
    }

    readCharNumber += length;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string returnString = peek_output(len);
    pop_output(len);
    return returnString;
}

void ByteStream::end_input() { _endInput = true; }

bool ByteStream::input_ended() const { return _endInput; }

size_t ByteStream::buffer_size() const { return writeCharNumber - readCharNumber; }

bool ByteStream::buffer_empty() const {  return buffer_size() == 0; }

bool ByteStream::eof() const { return buffer_empty() && input_ended(); }

size_t ByteStream::bytes_written() const { return writeCharNumber; }

size_t ByteStream::bytes_read() const { return readCharNumber; }

size_t ByteStream::remaining_capacity() const { return mCapacity - buffer_size(); }
