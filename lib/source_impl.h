/* -*- c++ -*- */
/*
 * Copyright 2023 Franco Venturi.
 *
 * based on:
 * - pcmcat in ka9q-radio
 * - wavfile_source block in GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef INCLUDED_RTP_SOURCE_IMPL_H
#define INCLUDED_RTP_SOURCE_IMPL_H

#include <gnuradio/rtp/source.h>

#include "multicast.h"

namespace gr {
namespace rtp {

struct pcmstream {
  uint32_t ssrc;            // RTP Sending Source ID
  int type;                 // RTP type (10,11,20)

  struct sockaddr sender;
  char addr[NI_MAXHOST];    // RTP Sender IP address
  char port[NI_MAXSERV];    // RTP Sender source port

  struct rtp_state rtp_state;
  int channels;
  int samprate;
};

template <class T>
class source_impl : public source<T>
{
private:
    int mcast_fd;
    struct pcmstream pcmstream;
    unsigned int ssrc; // Requested SSRC
    bool quiet;

public:
    source_impl(const std::string& mcast_address, unsigned int ssrc, int channels=1, bool quiet=false);
    ~source_impl();

    int sample_rate() const override { return pcmstream.samprate; };

    int bits_per_sample() const override { return sizeof(std::int16_t) * 8; }

    int channels() const override { return pcmstream.channels; };

    void set_ssrc(unsigned int ssrc) override {
        this->ssrc = ssrc;
        pcmstream.ssrc = 0;
    };

    unsigned int get_ssrc() const override { return ssrc; };

    int work(int noutput_items,
             gr_vector_const_void_star& input_items,
             gr_vector_void_star& output_items);

private:
    void check_channels(int channels) const { return; }
    int get_output_items(int sampcount, int channels, int noutput_channels, int time_step) const {
        return time_step + sampcount / channels;  // == sampcount for mono, sampcount/2 for stereo
    }
    int output_zeroes(int nzeroes, int channels, T** outs,
                      int noutput_items, int noutput_channels,
                      int offset = 0) const;
    int output_samples(const void *dp, int size, int channels, T** outs,
                       int noutput_items, int noutput_channels,
                       int offset = 0) const;

};

} // namespace rtp
} // namespace gr

#endif /* INCLUDED_RTP_SOURCE_IMPL_H */
