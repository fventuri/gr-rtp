/* -*- c++ -*- */
/*
 * Copyright 2023 Franco Venturi.
 * Copyright 2023 Phil Karn.
 *
 * based on:
 * - pcmcat in ka9q-radio
 * - wavfile_source block in GNU Radio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "source_impl.h"
#include <gnuradio/io_signature.h>

// Config constants
static int const Bufsize = 2048;

namespace gr {
namespace rtp {

template <typename T>
typename source<T>::sptr source<T>::make(const std::string& mcast_address, unsigned int ssrc, int channels, bool quiet)
{
    return gnuradio::make_block_sptr<source_impl<T>>(mcast_address, ssrc, channels, quiet);
}

template <typename T>
source_impl<T>::source_impl(const std::string& mcast_address, unsigned int ssrc, int channels, bool quiet)
    : gr::sync_block("rtp_source",
                     gr::io_signature::make(0, 0, 0),
                     gr::io_signature::make(channels, channels, sizeof(T))),
      ssrc(ssrc),
      quiet(quiet),
      mcast_fd(-1),
      rtp{}, // Init with zeros
      sessions(0),
      pcmstream(nullptr)
{
    check_channels(channels);
    // Set up multicast input
    mcast_fd = setup_mcast_in(mcast_address.c_str(), NULL, 0);
    if (mcast_fd == -1) {
        auto error_message = std::string("Can't set up input from \"") + mcast_address + "\"";
        this->d_logger->error(error_message);
        throw std::runtime_error(error_message);
    }
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
    int size = recvfrom(mcast_fd, buffer, sizeof(buffer), 0, &sender, &socksize);
    if (size == -1) {
        if (errno != EINTR) { // Happens routinely
            perror("recvmsg");
            usleep(1000);
        }
        return 0;
    }
    if(size < RTP_MIN_SIZE) {
        return 0; // Too small to be valid RTP
    }

    auto dp = static_cast<uint8_t const *>(ntoh_rtp(&rtp, buffer));
    size -= dp - buffer;
    if (rtp.pad) {
        // Remove padding
        size -= dp[size-1];
        rtp.pad = 0;
    }
    if (size <= 0) {
        return 0;
    }

    struct pcmstream *sp = lookup_session(&sender, rtp.ssrc, &pcmstream);
    if (sp == NULL) {
        // Not found
        if (sessions || (ssrc !=0 && rtp.ssrc != ssrc)) {
        // Only take specified SSRC or first SSRC for now
            return 0;
        }
        if ((sp = make_session(&sender, rtp.ssrc, rtp.seq, rtp.timestamp, &pcmstream)) == NULL){
            this->d_logger->warn("No room for new session!!");
            return 0;
        }
        getnameinfo((struct sockaddr *)&sender, sizeof(sender), sp->addr, sizeof(sp->addr),
                    //                sp->port, sizeof(sp->port), NI_NOFQDN|NI_DGRAM|NI_NUMERICHOST);
                    sp->port, sizeof(sp->port), NI_NOFQDN|NI_DGRAM);

        if (!quiet) {
            std::string info;
            switch (rtp.type) {
            case PCM_STEREO_PT:
            case PCM_STEREO_24_PT:
            case PCM_STEREO_16_PT:
            case PCM_STEREO_12_PT:
            case PCM_STEREO_8_PT:
                info = ", pcm stereo" + stereo_action(output_items.size());
                break;
            case PCM_MONO_PT:
            case PCM_MONO_24_PT:
            case PCM_MONO_16_PT:
            case PCM_MONO_12_PT:
            case PCM_MONO_8_PT:
                info = ", pcm mono";
                info = ", pcm mono" + mono_action(output_items.size());
                break;
            }
            this->d_logger->info("New session from {}@{}:{}, type {}{}", sp->ssrc, sp->addr, sp->port, rtp.type, info);
        }
        sessions++;
    }

    int samples_skipped = rtp_process(&sp->rtp_state, &rtp, 0); // get rid of last arg
    if (samples_skipped < 0) {
        return 0; // old dupe? What if it's simply out of sequence?
    }

    sp->type = rtp.type;
    noutput_items = output_samples(dp, size, rtp.type, outs, noutput_items,
                                   output_items.size());

    // Tell runtime system how many output items we produced.
    return noutput_items;
}

template<>
void source_impl<gr_complex>::check_channels(int channels) const
{
    if (channels > 1) {
        throw std::runtime_error("gr_complex requires only 1 output");
    }
}

template<>
std::string source_impl<float>::stereo_action(int n_out_chans) const
{
    if (n_out_chans == 1) {
        return ", values will be interleaved";
    } else {
        return "";
    }
}

template<>
std::string source_impl<std::int16_t>::stereo_action(int n_out_chans) const
{
    if (n_out_chans == 1) {
        return ", values will be interleaved";
    } else {
        return "";
    }
}

template<>
std::string source_impl<gr_complex>::mono_action(int n_out_chans) const
{
    return ", imaginary part will be 0";
}

template <>
int source_impl<gr_complex>::output_samples(const void *dp,
                                            int size,
                                            uint8_t type,
                                            gr_complex** outs,
                                            int noutput_items,
                                            int n_out_chans) const
{
    auto out = outs[0];

    int samples = 0;

    auto sdp = static_cast<const int16_t *>(dp);
    switch (type) {
    case PCM_STEREO_PT:
    case PCM_STEREO_24_PT:
    case PCM_STEREO_16_PT:
    case PCM_STEREO_12_PT:
    case PCM_STEREO_8_PT:

        samples = size / 4;
        if (samples > noutput_items) {
            this->d_logger->warn("work buffer not large enough - dropping samples. Buffer size={} Available samples={}", noutput_items, samples);
            samples = noutput_items;
        }
        for (int i = 0; i < samples; i++) {
            // Swap sample to host order
            int16_t left = ntohs(*sdp++);
            int16_t right = ntohs(*sdp++);
            out[i].real(static_cast<float>(left) / 32767.0f);
            out[i].imag(static_cast<float>(right) / 32767.0f);
        }
        break;
    case PCM_MONO_PT: // Mono
    case PCM_MONO_24_PT:
    case PCM_MONO_16_PT:
    case PCM_MONO_12_PT:
    case PCM_MONO_8_PT:

        samples = size / 2;
        if (samples > noutput_items) {
            this->d_logger->warn("work buffer not large enough - dropping samples. Buffer size={} Available samples={}", noutput_items, samples);
            samples = noutput_items;
        }
        for (int i = 0; i < samples; i++) {
            // Swap sample to host order
            int16_t d = ntohs(*sdp++);
            out[i].real(static_cast<float>(d) / 32767.0f);
            out[i].imag(0.0);
        }
        break;
    default:
        samples = 0;
        break; // ignore
    }

    return samples * n_out_chans;
}

template <>
int source_impl<float>::output_samples(const void *dp,
                                       int size,
                                       uint8_t type,
                                       float** outs,
                                       int noutput_items,
                                       int n_out_chans) const
{
    int samples = 0;

    if (n_out_chans == 1) {
        auto out = outs[0];
        auto sdp = static_cast<const int16_t *>(dp);
        switch (type) {
        case PCM_STEREO_PT:
        case PCM_STEREO_24_PT:
        case PCM_STEREO_16_PT:
        case PCM_STEREO_12_PT:
        case PCM_STEREO_8_PT:
        case PCM_MONO_PT: // Mono
        case PCM_MONO_24_PT:
        case PCM_MONO_16_PT:
        case PCM_MONO_12_PT:
        case PCM_MONO_8_PT:

            samples = size / 2;
            if (samples > noutput_items) {
                this->d_logger->warn("work buffer not large enough - dropping samples. Buffer size={} Available samples={}", noutput_items, samples);
                samples = noutput_items;
            }
            for (int i = 0; i < samples; i++) {
                // Swap sample to host order
                int16_t d = ntohs(*sdp++);
                out[i] = static_cast<float>(d) / 32767.0f;
            }
            break;
        default:
            samples = 0;
            break; // ignore
        }

    } else if (n_out_chans == 2) {
        auto out_left = outs[0];
        auto out_right = outs[1];
        auto sdp = static_cast<const int16_t *>(dp);
        switch (type) {
        case PCM_STEREO_PT:
        case PCM_STEREO_24_PT:
        case PCM_STEREO_16_PT:
        case PCM_STEREO_12_PT:
        case PCM_STEREO_8_PT:

            samples = size / 4;
            if (samples > noutput_items) {
                this->d_logger->warn("work buffer not large enough - dropping samples. Buffer size={} Available samples={}", noutput_items, samples);
                samples = noutput_items;
            }
            for (int i = 0; i < samples; i++) {
                // Swap sample to host order
                int16_t left = ntohs(*sdp++);
                int16_t right = ntohs(*sdp++);
                out_left[i] = static_cast<float>(left) / 32767.0f;
                out_right[i] = static_cast<float>(right) / 32767.0f;
            }
            break;
        case PCM_MONO_PT: // Mono
        case PCM_MONO_24_PT:
        case PCM_MONO_16_PT:
        case PCM_MONO_12_PT:
        case PCM_MONO_8_PT:

            samples = size / 2;
            if (samples > noutput_items) {
                this->d_logger->warn("work buffer not large enough - dropping samples. Buffer size={} Available samples={}", noutput_items, samples);
                samples = noutput_items;
            }
            for (int i = 0; i < samples; i++) {
                // Swap sample to host order
                int16_t d = ntohs(*sdp++);
                float df  = static_cast<float>(d) / 32767.0f;
                out_left[i] = df;
                out_right[i] = df;
            }
            break;
        default:
            samples = 0;
            break; // ignore
        }

    } else {
        samples = 0;
    }

    return samples * n_out_chans;
}

template <>
int source_impl<std::int16_t>::output_samples(const void *dp,
                                              int size,
                                              uint8_t type,
                                              std::int16_t** outs,
                                              int noutput_items,
                                              int n_out_chans) const
{
    int samples = 0;

    if (n_out_chans == 1) {
        auto out = outs[0];
        auto sdp = static_cast<const int16_t *>(dp);
        switch (type) {
        case PCM_STEREO_PT:
        case PCM_STEREO_24_PT:
        case PCM_STEREO_16_PT:
        case PCM_STEREO_12_PT:
        case PCM_STEREO_8_PT:
        case PCM_MONO_PT: // Mono
        case PCM_MONO_24_PT:
        case PCM_MONO_16_PT:
        case PCM_MONO_12_PT:
        case PCM_MONO_8_PT:

            samples = size / 2;
            if (samples > noutput_items) {
                this->d_logger->warn("work buffer not large enough - dropping samples. Buffer size={} Available samples={}", noutput_items, samples);
                samples = noutput_items;
            }
            for (int i = 0; i < samples; i++) {
                // Swap sample to host order
                int16_t d = ntohs(*sdp++);
                out[i] = d;
            }
            break;
        default:
            samples = 0;
            break; // ignore
        }

    } else if (n_out_chans == 2) {
        auto out_left = outs[0];
        auto out_right = outs[1];
        auto sdp = static_cast<const int16_t *>(dp);
        switch (type) {
        case PCM_STEREO_PT:
        case PCM_STEREO_24_PT:
        case PCM_STEREO_16_PT:
        case PCM_STEREO_12_PT:
        case PCM_STEREO_8_PT:

            samples = size / 4;
            if (samples > noutput_items) {
                this->d_logger->warn("work buffer not large enough - dropping samples. Buffer size={} Available samples={}", noutput_items, samples);
                samples = noutput_items;
            }
            for (int i = 0; i < samples; i++) {
                // Swap sample to host order
                int16_t left = ntohs(*sdp++);
                int16_t right = ntohs(*sdp++);
                out_left[i] = left;
                out_right[i] = right;
            }
            break;
        case PCM_MONO_PT: // Mono
        case PCM_MONO_24_PT:
        case PCM_MONO_16_PT:
        case PCM_MONO_12_PT:
        case PCM_MONO_8_PT:

            samples = size / 2;
            if (samples > noutput_items) {
                this->d_logger->warn("work buffer not large enough - dropping samples. Buffer size={} Available samples={}", noutput_items, samples);
                samples = noutput_items;
            }
            for (int i = 0; i < samples; i++) {
                // Swap sample to host order
                int16_t d = ntohs(*sdp++);
                out_left[i] = d;
                out_right[i] = d;
            }
            break;
        default:
            samples = 0;
            break; // ignore
        }

    } else {
        samples = 0;
    }

    return samples * n_out_chans;
}

template class source<gr_complex>;
template class source<float>;
template class source<std::int16_t>;
} /* namespace rtp */
} /* namespace gr */
