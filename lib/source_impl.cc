/* -*- c++ -*- */
/*
 * Copyright 2023 Franco Venturi.
 * Copyright 2023 Phil Karn, KA9Q
 *
 * based on:
 * - pcmcat in ka9q-radio
 * - wavfile_source block in GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "source_impl.h"
#include <gnuradio/io_signature.h>

namespace gr {
namespace rtp {

// Config constants
static int const Bufsize = 9000; // allow for jumbograms

static struct timeval udp_timeout = {0, 100000};   // set timeout to 0.1s

// internal functions defined below
static void init(struct pcmstream *pc, struct rtp_header const *rtp,
                 struct sockaddr const *sender);

template <typename T>
typename source<T>::sptr source<T>::make(const std::string& mcast_address,
                                         unsigned int ssrc,
                                         int in_channels,
                                         int out_channels,
                                         bool quiet)
{
    return gnuradio::make_block_sptr<source_impl<T>>(mcast_address,
                                                     ssrc,
                                                     in_channels,
                                                     out_channels,
                                                     quiet);
}

template <typename T>
source_impl<T>::source_impl(const std::string& mcast_address,
                            unsigned int ssrc,
                            int in_channels,
                            int out_channels,
                            bool quiet)
    : gr::sync_block("rtp_source",
                     gr::io_signature::make(0, 0, 0),
                     gr::io_signature::make(out_channels, out_channels, sizeof(T))),
      mcast_fd(-1),
      pcmstream{}, // Init with zeros
      ssrc(ssrc),
      channels(in_channels),
      quiet(quiet)
{
    check_out_channels(out_channels);
    // Set up multicast input
    mcast_fd = setup_mcast_in(mcast_address.c_str(), NULL, 0);
    if (mcast_fd == -1) {
        auto error_message = std::string("Can't set up input from \"") + mcast_address + "\"";
        this->d_logger->error(error_message);
        throw std::runtime_error(error_message);
    }
    // set UDP socket timeout so it can be interrupted by Boost
    setsockopt(mcast_fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&udp_timeout, sizeof(udp_timeout));
    //this->set_min_noutput_items(1200);
}

template <typename T>
source_impl<T>::~source_impl() {}

template <typename T>
int source_impl<T>::work(int noutput_items,
                         gr_vector_const_void_star& input_items,
                         gr_vector_void_star& output_items)
{
    auto outs = reinterpret_cast<T**>(&output_items[0]);

    // audio input thread
    // Receive audio multicasts, multiplex into sessions, send to output
    // What do we do if we get different streams?? think about this
    struct sockaddr sender;
    socklen_t socksize = sizeof(sender);
    uint8_t buffer[Bufsize];
    // Gets all packets to multicast destination address, regardless of sender IP, sender port, dest port, ssrc
    int size;
    while (true) {
        boost::this_thread::interruption_point();
        size = recvfrom(mcast_fd, buffer, sizeof(buffer), 0, &sender, &socksize);

        if (size == -1) {
            perror("recvmsg");
            return 0;
        }
        if(size < RTP_MIN_SIZE) {
            continue; // Too small to be valid RTP
        }

        struct rtp_header rtp;
        auto dp = static_cast<uint8_t const *>(ntoh_rtp(&rtp, buffer));

        size -= dp - buffer;
        if (rtp.pad) {
            // Remove padding
            size -= dp[size-1];
            rtp.pad = 0;
        }
        if (size <= 0) {
            continue;
        }

        if (rtp.ssrc == 0 || (ssrc != 0 && rtp.ssrc != ssrc)) {
           continue; // Ignore unwanted or invalid SSRCs
        }

        if (pcmstream.ssrc == 0) {
            // First packet on stream, initialize
            init(&pcmstream, &rtp, &sender);

            if (!quiet) {
                this->d_logger->info("New session from {}@{}:{}",
                                     pcmstream.ssrc,
                                     pcmstream.addr,
                                     pcmstream.port);
            }
        } else if (rtp.ssrc != pcmstream.ssrc) {
            continue; // unwanted SSRC, ignore
        }

        if (!address_match(&sender, &pcmstream.sender) || getportnumber(&pcmstream.sender) != getportnumber(&sender)) {
            // Source changed, the sender restarted
            init(&pcmstream, &rtp, &sender);
            if (!quiet) {
                this->d_logger->info("Session restart from {}@{}:{}",
                                     pcmstream.ssrc,
                                     pcmstream.addr,
                                     pcmstream.port);
            }
        }
        if (rtp.marker) {
            pcmstream.rtp_state.timestamp = rtp.timestamp;      // Resynch
        }

        int const sampcount = size / sizeof(int16_t); // # of 16-bit samples, regardless of mono or stereo
        int const framecount = sampcount / channels; // == sampcount for mono, sampcount/2 for stereo
        // fv
        //this->d_logger->info("noutput_items={} noutput_channels={} sampcount={} framecount={}", noutput_items, output_items.size(), sampcount, framecount);
        int offset = 0;

        int const time_step = rtp.timestamp - pcmstream.rtp_state.timestamp;
        if (time_step < 0) {
            // Old dupe
            pcmstream.rtp_state.dupes++;
            this->d_logger->info("Out of order samples - {} received after {}", rtp.timestamp, pcmstream.rtp_state.timestamp);
            continue;
        } else if (time_step > 0) {
            pcmstream.rtp_state.drops++;
            this->d_logger->info("Dropped {} samples - from {} to {}", time_step, pcmstream.rtp_state.timestamp, rtp.timestamp);
            int const nexpected_output_items = get_output_items(sampcount, channels, output_items.size(), time_step);
            if (nexpected_output_items <= noutput_items) {  // Arbitrary threshold - clean this up!
                offset = output_zeroes(time_step, channels, outs,
                                       noutput_items, output_items.size());
            }
            // Resync
            pcmstream.rtp_state.timestamp = rtp.timestamp; // Bring up to date?
        }
        pcmstream.rtp_state.bytes += size;

        noutput_items = output_samples(dp, size, channels, outs, noutput_items,
                                       output_items.size(), offset);

        pcmstream.rtp_state.timestamp += framecount;
        pcmstream.rtp_state.seq = rtp.seq + 1;

        // Tell runtime system how many output items we produced.
        return noutput_items;
    }
}

template<>
void source_impl<gr_complex>::check_out_channels(int channels) const
{
    if (channels > 1) {
        throw std::runtime_error("gr_complex requires only 1 output");
    }
}

template<>
int source_impl<std::int16_t>::get_output_items(int sampcount, int channels, int noutput_channels, int time_step) const
{
    if (channels == 2 && noutput_channels == 1) {
        return time_step * channels + sampcount;  // interleaved short case
    } else {
        return time_step + sampcount / channels;  // == sampcount for mono, sampcount/2 for stereo
    }
}

template <>
int source_impl<gr_complex>::output_zeroes(int nzeroes,
                                           int channels,
                                           gr_complex** outs,
                                           int noutput_items,
                                           int noutput_channels,
                                           int offset) const
{
    auto out = outs[0];

    if (offset + nzeroes > noutput_items) {
        this->d_logger->warn("work buffer not large enough - dropping zeroes - buffer size={} nzeroes={} offset={}", noutput_items, nzeroes, offset);
        nzeroes = noutput_items - offset;
    }

    std::fill_n(out + offset, nzeroes, 0);
    return offset + nzeroes;
}

template <>
int source_impl<float>::output_zeroes(int nzeroes,
                                      int channels,
                                      float** outs,
                                      int noutput_items,
                                      int noutput_channels,
                                      int offset) const
{
    if (offset + nzeroes > noutput_items) {
        this->d_logger->warn("work buffer not large enough - dropping zeroes - buffer size={} nzeroes={} offset={}", noutput_items, nzeroes, offset);
        nzeroes = noutput_items - offset;
    }
    if (noutput_channels == 1) {
        auto out = outs[0];
        std::fill_n(out + offset, nzeroes, 0);
    } else if (noutput_channels == 2) {
        auto out_left = outs[0];
        auto out_right = outs[1];
        std::fill_n(out_left + offset, nzeroes, 0);
        std::fill_n(out_right + offset, nzeroes, 0);
    } else {
        nzeroes = 0;
    }
    return offset + nzeroes;
}

template <>
int source_impl<std::int16_t>::output_zeroes(int nzeroes,
                                             int channels,
                                             std::int16_t** outs,
                                             int noutput_items,
                                             int noutput_channels,
                                             int offset) const
{
    // interleaved short case
    if (channels == 2 && noutput_channels == 1) {
        nzeroes *= 2;
    }
    if (offset + nzeroes > noutput_items) {
        this->d_logger->warn("work buffer not large enough - dropping zeroes - buffer size={} nzeroes={} offset={}", noutput_items, nzeroes, offset);
        nzeroes = noutput_items - offset;
        // interleaved short case - make sure nzeroes is even
        if (channels == 2 && noutput_channels == 1) {
            nzeroes -= nzeroes % 2;
        }
    }
    if (noutput_channels == 1) {
        auto out = outs[0];
        std::fill_n(out + offset, nzeroes, 0);
    } else if (noutput_channels == 2) {
        auto out_left = outs[0];
        auto out_right = outs[1];
        std::fill_n(out_left + offset, nzeroes, 0);
        std::fill_n(out_right + offset, nzeroes, 0);
    } else {
        nzeroes = 0;
    }
    return offset + nzeroes;
}

template <>
int source_impl<gr_complex>::output_samples(const void *dp,
                                            int size,
                                            int channels,
                                            gr_complex** outs,
                                            int noutput_items,
                                            int noutput_channels,
                                            int offset) const
{
    auto out = outs[0];

    int samples = size / (sizeof(int16_t) * channels);
    if (offset + samples > noutput_items) {
        this->d_logger->warn("work buffer not large enough - dropping samples - buffer size={} samples={} offset={}", noutput_items, samples, offset);
        samples = noutput_items - offset;
    }

    auto sdp = static_cast<const int16_t *>(dp);
    if (channels == 1) {
        for (int i = offset; i < offset + samples; i++) {
            // Swap sample to host order
            int16_t d = ntohs(*sdp++);
            out[i].real(static_cast<float>(d) / 32767.0f);
            out[i].imag(0.0);
        }
    } else if (channels == 2) {
        for (int i = offset; i < offset + samples; i++) {
            // Swap sample to host order
            int16_t left = ntohs(*sdp++);
            int16_t right = ntohs(*sdp++);
            out[i].real(static_cast<float>(left) / 32767.0f);
            out[i].imag(static_cast<float>(right) / 32767.0f);
        }
    } else {
        samples = 0;
    }

    return offset + samples;
}

template <>
int source_impl<float>::output_samples(const void *dp,
                                       int size,
                                       int channels,
                                       float** outs,
                                       int noutput_items,
                                       int noutput_channels,
                                       int offset) const
{
    int samples = size / (sizeof(int16_t) * channels);
    if (offset + samples > noutput_items) {
        this->d_logger->warn("work buffer not large enough - dropping samples - buffer size={} samples={} offset={}", noutput_items, samples, offset);
        samples = noutput_items - offset;
    }

    auto sdp = static_cast<const int16_t *>(dp);
    if (noutput_channels == 1) {
        auto out = outs[0];
        if (channels == 1) {
            for (int i = offset; i < offset + samples; i++) {
                // Swap sample to host order
                int16_t d = ntohs(*sdp++);
                out[i] = static_cast<float>(d) / 32767.0f;
            }
        } else if (channels == 2) {
            // Downmix to mono
            for (int i = offset; i < offset + samples; i++) {
                // Swap sample to host order
                int16_t left = ntohs(*sdp++);
                int16_t right = ntohs(*sdp++);
                float leftf = static_cast<float>(left) / 32767.0f;
                float rightf = static_cast<float>(right) / 32767.0f;
                out[i] = (leftf + rightf) / 2.0;
            }
        } else {
            samples = 0;
        }
    } else if (noutput_channels == 2) {
        auto out_left = outs[0];
        auto out_right = outs[1];
        if (channels == 1) {
            // Expand to pseudo-stereo
            for (int i = offset; i < offset + samples; i++) {
                // Swap sample to host order
                int16_t d = ntohs(*sdp++);
                float df  = static_cast<float>(d) / 32767.0f;
                out_left[i] = df;
                out_right[i] = df;
            }
        } else if (channels == 2) {
            for (int i = offset; i < offset + samples; i++) {
                // Swap sample to host order
                int16_t left = ntohs(*sdp++);
                int16_t right = ntohs(*sdp++);
                out_left[i] = static_cast<float>(left) / 32767.0f;
                out_right[i] = static_cast<float>(right) / 32767.0f;
            }
        } else {
            samples = 0;
        }
    } else {
        samples = 0;
    }

    return offset + samples;
}

template <>
int source_impl<std::int16_t>::output_samples(const void *dp,
                                              int size,
                                              int channels,
                                              std::int16_t** outs,
                                              int noutput_items,
                                              int noutput_channels,
                                              int offset) const
{
    int samples = size / (sizeof(int16_t) * channels);
    // interleaved short case
    if (channels == 2 && noutput_channels == 1) {
        samples = size / sizeof(int16_t);
    }
    if (offset + samples > noutput_items) {
        this->d_logger->warn("work buffer not large enough - dropping samples - buffer size={} samples={} offset={}", noutput_items, samples, offset);
        samples = noutput_items - offset;
    }

    auto sdp = static_cast<const int16_t *>(dp);
    if (noutput_channels == 1) {
        auto out = outs[0];
        if (channels == 1 || channels == 2) {
            // (in) channels == 1 -> standard mono
            // (in) channels == 2 -> interleaved shorts (for raw I/Q)
            for (int i = offset; i < offset + samples; i++) {
                // Swap sample to host order
                int16_t d = ntohs(*sdp++);
                out[i] = d;
            }
        } else {
            samples = 0;
        }
    } else if (noutput_channels == 2) {
        auto out_left = outs[0];
        auto out_right = outs[1];
        if (channels == 1) {
            // Expand to pseudo-stereo
            for (int i = offset; i < offset + samples; i++) {
                // Swap sample to host order
                int16_t d = ntohs(*sdp++);
                out_left[i] = d;
                out_right[i] = d;
            }
        } else if (channels == 2) {
            for (int i = offset; i < offset + samples; i++) {
                // Swap sample to host order
                int16_t left = ntohs(*sdp++);
                int16_t right = ntohs(*sdp++);
                out_left[i] = left;
                out_right[i] = right;
            }
        } else {
            samples = 0;
        }
    } else {
        samples = 0;
    }

    return offset + samples;
}

static void init(struct pcmstream *pc, struct rtp_header const *rtp,
                 struct sockaddr const *sender) {
    // First packet on stream, initialize
    pc->ssrc = rtp->ssrc;
    pc->type = rtp->type;

    memcpy(&pc->sender,sender,sizeof(pc->sender)); // Remember sender
    getnameinfo((struct sockaddr *)&pc->sender, sizeof(pc->sender),
                pc->addr,sizeof(pc->addr),
                pc->port,sizeof(pc->port), NI_NOFQDN | NI_DGRAM);
    pc->rtp_state.timestamp = rtp->timestamp;
    pc->rtp_state.seq = rtp->seq;
    pc->rtp_state.packets = 0;
    pc->rtp_state.bytes = 0;
    pc->rtp_state.drops = 0;
    pc->rtp_state.dupes = 0;
}

template class source<gr_complex>;
template class source<float>;
template class source<std::int16_t>;
} /* namespace rtp */
} /* namespace gr */
