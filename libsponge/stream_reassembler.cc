#include "stream_reassembler.hh"

#include <iostream>
#include <unordered_set>
#include <vector>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _unsorted_data(), _remaining_capacity(capacity) {}

void StreamReassembler::add_unsorted_to_output() {
    std::vector<size_t> vec;
    for (auto &i : _unsorted_data){

        if (i.first == _next_index) {
            vec.emplace_back(i.first);
            insert_data(i.second);
        }
    }
    for (auto &key : vec) {
        _unsorted_data.erase(key);
    }
}

void StreamReassembler::insert_data(const string &data) {

    size_t added_volume = _output.write(data);
    _next_index = _next_index + added_volume;
}

void StreamReassembler::update_remain_capacity() {

    _remaining_capacity = _next_index + _output.remaining_capacity();
}

void StreamReassembler::insert_into_unsorted(const string &data, const size_t index) {

    string _str;
    size_t _index;
    if(index <= _next_index){

        _str = string().assign(data.begin() + _next_index - index, data.begin() + data.size());
        _index = _next_index;
    }else{

        _str = string().assign(data.begin(), data.begin() + data.size());
        _index = index;
    }

    size_t _index_of_end = _index + _str.size();
    std::vector<size_t> vec;
    for (auto &i : _unsorted_data){

        size_t _index_of_end_of_i = i.first + i.second.size();
        if (i.first <= _index && _index_of_end <= _index_of_end_of_i){

            return;
        }else if(_index + 1 <= _index_of_end_of_i && _index >= i.first){

            _str = string().assign(_str.begin() + _index_of_end_of_i - _index, _str.begin() + _str.size());
            _index = _index_of_end_of_i;
        }else if(_index_of_end <= _index_of_end_of_i && _index_of_end >= i.first + 1){

            _str = string().assign(_str.begin(), _str.begin() + _str.size() - (_index_of_end - i.first)); 
        }else if(_index_of_end >= _index_of_end_of_i && _index <= i.first){

            vec.emplace_back(i.first);
        }
    }

    for (auto &key : vec) {
        _unsorted_data.erase(key);
    }

    _unsorted_data[_index] = _str;
}
//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof) {
        _eof_flag = true;
        _eof_index = index + data.length();
    }
    update_remain_capacity();

    size_t _data_end_index = index + data.size(); 
    
    if (data.size() != 0){

        if (_data_end_index <= _next_index) {

            return;
        }
        if (_data_end_index > _remaining_capacity){

            string str = string().assign(data.begin(), data.begin() + data.size() - (_data_end_index - _remaining_capacity));
            insert_into_unsorted(str, index);
        }else{
            
            insert_into_unsorted(data, index);
        }

        add_unsorted_to_output();
    }
    if ((_next_index == _eof_index) && _eof_flag) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t length = 0;
    for (auto &p : _unsorted_data) {
        length += p.second.size();
    }
    return length;
}

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }
