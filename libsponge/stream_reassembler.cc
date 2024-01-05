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
    , _eof_index()
    , _unassembled_string(map<size_t, string>())
    , _assembled_string(string()) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    string _data = data;
    _first_unaccepted = max(_first_unaccepted, index + _data.size());
    if (!_unassembled_string.empty() && _unassembled_string.begin()->first <= index + _data.size()) {
        auto max_substring = _unassembled_string.begin();
        for (auto iter = _unassembled_string.begin(); iter != _unassembled_string.end() &&
                                                      iter->first <= index + _data.size() &&
                                                      iter->first + iter->second.size() >= index + _data.size();) {
            if (iter->first + iter->second.size() > max_substring->first + max_substring->second.size()) {
                max_substring = iter;
                ++iter;
            } else {
                iter = _unassembled_string.erase(
                    iter);  // erase operatioin returns the next valid iterator after being erased
            }
        }
        if (max_substring->first + max_substring->second.size() >= index + _data.size()) {
            _data.append(string(max_substring->second.begin() + (index + _data.size() - max_substring->first),
                                max_substring->second.end()));
            if (!_unassembled_string.empty() && max_substring != _unassembled_string.begin())
                _unassembled_string.erase(_unassembled_string.begin());
            else if (!_unassembled_string.empty()) {
                _unassembled_string.erase(max_substring);
            }
        }
    }
    // _data = removeNullCharacters(_data);
    // Make sure first_unacceptable - first_unread >= capacity
    // while (_first_unaccepted - _first_unread > _capacity) {
    //     _first_unread = _output.bytes_read();
    // }
    if (index == _first_unassembled) {
        _assembled_string += _data;
        _first_unassembled += _data.size();
    } else if (index < _first_unassembled) {
        if (index + _data.size() <= _first_unassembled) {
            return;
        } else {
            _assembled_string += string(_data.begin() + (_first_unassembled - index), _data.end());
            push_to_stream(_assembled_string);
            _first_unassembled = index + _data.size();
        }
    } else {
        _unassembled_string[index] =
            _unassembled_string[index].size() > _data.size() ? _unassembled_string[index] : _data;
    }
    push_to_stream(_assembled_string);
    if (eof) {
        _eof_index = std::make_optional<size_t>(index + _data.size());
    }
    if (_eof_index.has_value() && _first_unassembled - _assembled_string.size() >= _eof_index.value()) {
        _output.end_input();
    }
    // std::cout << "data:" << data << "\n index:" << index << "\neof:" << eof << "\n_data:" << _data << std::endl;
}

size_t StreamReassembler::unassembled_bytes() const { return _first_unaccepted - _first_unassembled; }

bool StreamReassembler::empty() const { return _first_unaccepted == _first_unassembled; }

size_t StreamReassembler::push_to_stream(string &assembled_string) {
    auto len = _output.write(assembled_string);
    if (len == assembled_string.size()) {
        assembled_string.clear();  // Not sure what if we use iterator like string(end(), end()),thus using clear here
    } else {
        assembled_string = string(assembled_string.begin() + len, assembled_string.end());
    }
    return len;
}
std::string removeNullCharacters(const std::string &str) {
    std::string result;
    for (char c : str) {
        if (c != '\000') {
            result += c;
        }
    }
    return result;
}