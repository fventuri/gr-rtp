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
#include "session.h"

namespace gr {
namespace rtp {

template <class T>
class source_impl : public source<T>
{
private:
    unsigned int ssrc; // Requested SSRC
    bool quiet;
    int mcast_fd;
    struct rtp_header rtp;
    int sessions; // Session count - limit to 1 for now
    struct pcmstream *pcmstream;

public:
    source_impl(const std::string& mcast_address, unsigned int ssrc, int channels=1, bool quiet=false);
    ~source_impl();

    int sample_rate() const override { return samprate_from_pt(rtp.type); };

    int bits_per_sample() const override { return sizeof(std::int16_t) * 8; }

    int channels() const override { return channels_from_pt(rtp.type); };

    int work(int noutput_items,
             gr_vector_const_void_star& input_items,
             gr_vector_void_star& output_items);

private:
    void check_channels(int channels) const { return; }
    std::string stereo_action(int n_out_chans) const { return ""; }
    std::string mono_action(int n_out_chans) const { return ""; }
    int output_samples(const void *dp, int size, uint8_t type, T** outs,
                       int noutput_items, int n_out_chans) const;

};

} // namespace rtp
} // namespace gr

#endif /* INCLUDED_RTP_SOURCE_IMPL_H */
