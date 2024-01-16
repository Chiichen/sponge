#include "tcp_receiver.hh"

#include "wrapping_integers.hh"

#include <optional>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    auto header = seg.header();

    if (!_is_syn_received) {
        if (!header.syn) {
            return;
        }
        _is_syn_received = true;
        _isn = seg.header().seqno;
    }
    auto index = unwrap(seg.header().seqno, _isn, _reassembler.stream_out().bytes_written() + 1);  // Skip Sync package
    auto cur_index = header.syn ? index : index - 1;
    return _reassembler.push_substring(seg.payload().copy(), cur_index, header.fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_is_syn_received) {
        return nullopt;
    }
    auto ackno = _reassembler.stream_out().bytes_written() + 1;  // Received Syn , n-datas with n+1 package
    ackno = _reassembler.stream_out().input_ended()
                ? ackno + 1
                : ackno;  // Received Fin, n-datas with n+2 package. Looking forward Syn pack
    return wrap(ackno, _isn);
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
