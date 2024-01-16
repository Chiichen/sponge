#include "tcp_sender.hh"

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_header.hh"
#include "wrapping_integers.hh"

#include <cstdint>
#include <optional>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _retx_timer(nullopt)
    , _retx_timeout(retx_timeout) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    auto cur_window_size = max(static_cast<uint64_t>(1), _cur_window_size);
    while (cur_window_size > _bytes_in_flight) {  // If there is no data in flight, it means a SYN or FIn is needed
        auto segment = TCPSegment();
        if (next_seqno_absolute() == 0) {  // Send SYN immediately
            segment.header().syn = true;
            segment.header().seqno = next_seqno();
            _retx_timer = _retx_timer.has_value() ? _retx_timer : 0;
        } else {
            segment.header().seqno = next_seqno();
            auto payload_size = min(cur_window_size - _bytes_in_flight, TCPConfig::MAX_PAYLOAD_SIZE);
            segment.payload() = _stream.read(payload_size);
            if (stream_in().eof() &&
                next_seqno_absolute() != _stream.bytes_written() + 2) {  // Arrive EOF AND FIN not sent before
                if (segment.payload().size() + _bytes_in_flight + 1 <=
                    cur_window_size) {  // there is enough window space to obtain an extra FIN byte
                    segment.header().fin = true;
                }
            }
            if (segment.length_in_sequence_space() == 0) {
                return;  // If there is no data to be sent, cancel tramsmission
            }
        }
        _segments_out.push(segment);
        _segments_tracking.push(pair(next_seqno_absolute(), segment));
        _bytes_in_flight += segment.length_in_sequence_space();
        _next_seqno += segment.length_in_sequence_space();
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    auto abs_ackno = unwrap(ackno, _isn, _next_seqno);
    if (abs_ackno > _next_seqno) {  // Invalid package
        return;
    }
    auto queue_size = _segments_tracking.size();
    for (size_t i = 0; i < queue_size; i++) {
        if (_segments_tracking.front().first >= abs_ackno)
            break;
        if (abs_ackno >= _segments_tracking.front().first +
                             _segments_tracking.front().second.length_in_sequence_space()) {  // Fully Acked
            _bytes_in_flight -= _segments_tracking.front().second.length_in_sequence_space();
            _segments_tracking.pop();
            _retx_timer = 0;
            _retx_timeout = _initial_retransmission_timeout;
        }
    }
    // while (!_segments_tracking.empty() && _segments_tracking.front().first < abs_ackno) {  // Remove acked segment
    //     if (abs_ackno == _segments_tracking.front().first +
    //                          _segments_tracking.front().second.length_in_sequence_space()) {  // Fully Acked
    //         _bytes_in_flight -= _segments_tracking.front().second.length_in_sequence_space();
    //         _segments_tracking.pop();
    //         _retx_timer = 0;
    //     }
    // }
    _retx_count = 0;
    _cur_window_size = window_size;
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (_retx_timer.has_value()) {
        _retx_timer = _retx_timer.value() + ms_since_last_tick;
        if (_retx_timer >= _retx_timeout && !_segments_tracking.empty()) {
            _retx_count++;
            _retx_timeout =
                _cur_window_size != 0 ? _retx_timeout << 1 : _retx_timeout;  // DO NOT double RTO if window size is 0
            _retx_timer = 0;
            auto retx_segment = _segments_tracking.front().second;  // Resend the first segment
            _segments_out.push(retx_segment);
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _retx_count; }

void TCPSender::send_empty_segment() {
    auto segment = TCPSegment();
    segment.header().seqno = next_seqno();
    _segments_out.push(segment);
    _segments_tracking.push(pair(next_seqno_absolute(), segment));
    _bytes_in_flight += segment.length_in_sequence_space();
    _next_seqno += 1;
}
