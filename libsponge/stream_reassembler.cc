#include "stream_reassembler.hh"

#include <cstdio>
#include <iostream>
#include <optional>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity)
    , _capacity(capacity)
    , _first_unread(0)
    , _first_unassembled(0)
    , _first_unaccepted(0)
    , _next_assembled(0)
    , _eof_index()
    , _unassembled_string(map<size_t, string>())
    , _assembled_string(string())
    , _unassembled_bytes(0) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    string _data =
        index + data.size() - _output.bytes_read() > _capacity
            ? string(data.begin(),
                     min(data.end(),
                         data.end() - (index + data.size() - _output.bytes_read() - _capacity)))  // To avoid overflow
            : data;  // Truncate the overflowed part
    _first_unaccepted = max(_first_unaccepted, index + _data.size());
    if (!_unassembled_string.empty() && _unassembled_string.begin()->first <= index + _data.size()) {
        for (auto iter = _unassembled_string.begin(); iter != _unassembled_string.end();) {
            //     in _unassembled_string map
            //        |-------------|
            //    |-------------|
            //     data passed
            //  ------------- OR -------------------
            //     in _unassembled_string map
            //        |-------------|
            //    |-----------------------|
            //           data passed
            if (iter->first <= index + _data.size() && iter->first >= index) {
                if (iter->first + iter->second.size() >= index + _data.size()) {
                    _data.append(
                        string(iter->second.begin() + (index + _data.size() - iter->first), iter->second.end()));
                }
                _unassembled_bytes -= iter->second.size();
                iter = _unassembled_string.erase(
                    iter);  // erase operatioin returns the next valid iterator after being erased
                iter = _unassembled_string.begin();  // Maybe this can be deleted?
            }
            //    in _unassembled_string map
            //       |-------------|
            //             |-------------|
            //                data passed
            //  ------------- OR -------------------
            //    in _unassembled_string map
            //       |-----------------------|
            //             |-------------|
            //                data passed
            else if (index >= iter->first && index <= iter->first + iter->second.size()) {
                if (iter->first + iter->second.size() <= index + data.size()) {
                    auto iter_offset = data.begin() + (iter->first + iter->second.size() - index);
                    _unassembled_bytes += data.end() - iter_offset;
                    iter->second.append(string(iter_offset, data.end()));
                    return;
                } else {
                    return;
                }
            } else {
                iter++;
            }
            //    in _unassembled_string map
            //       |-------------|
            //                           |-------------|
            //                              data passed
            //              OR
            //         data passed
            //       |-------------|
            //                           |-------------|
            //                       in _unassembled_string map
            // just continue
        }
    }
    if (index == _next_assembled) {
        _assembled_string.append(_data);
        _next_assembled += _data.size();
    } else if (index < _next_assembled) {
        if (index + _data.size() <= _next_assembled) {
            return;
        } else {
            _assembled_string.append(string(_data.begin() + (_next_assembled - index), _data.end()));
            _next_assembled = index + _data.size();
        }
    } else {
        if (index > _next_assembled) {
            _first_unassembled = _first_unassembled > _next_assembled ? min(index, _first_unassembled) : index;
        }
        if (_unassembled_string[index].size() > _data.size()) {
            _unassembled_string[index] = _unassembled_string[index];
        } else {
            _unassembled_bytes += _data.size() - _unassembled_string[index].size();
            _unassembled_string[index] = _data;
        }
    }
    auto len = push_to_stream(_assembled_string);
    if (len != 0) {
        _first_unassembled = _next_assembled + _assembled_string.size();
    }
    if (eof) {
        _eof_index = std::make_optional<size_t>(index + data.size());
    }
    if (_eof_index.has_value() && _next_assembled - _assembled_string.size() >= _eof_index.value()) {
        _output.end_input();
    }
    // std::cout << "data:" << data << "\n index:" << index << "\neof:" << eof << "\n_data:" << _data << std::endl;
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled_bytes; }

bool StreamReassembler::empty() const { return _first_unaccepted == _next_assembled; }

size_t StreamReassembler::push_to_stream(string &assembled_string) {
    auto len = _output.write(assembled_string);
    if (len == assembled_string.size()) {
        assembled_string.clear();  // Not sure what if we use iterator like string(end(), end()),thus using clear here
    } else {
        assembled_string = string(assembled_string.begin() + len, assembled_string.end());
    }
    return len;
}
