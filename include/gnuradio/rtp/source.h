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

#ifndef INCLUDED_RTP_SOURCE_H
#define INCLUDED_RTP_SOURCE_H

#include <gnuradio/rtp/api.h>
#include <gnuradio/sync_block.h>

namespace gr {
namespace rtp {

/*!
 * \brief Read stream from an RTP PCM stream, output gr_complex or interleaved shorts
 * \ingroup rtp
 *
 * \details
 * Unless otherwise called, values are within [-1;1].
 * Check gr_make_rtp_source() for extra info.
 */
template <class T>
class RTP_API source : virtual public gr::sync_block
{
public:
    // gr::rtp:source::sptr
    typedef std::shared_ptr<source<T>> sptr;

    static sptr make(const std::string& mcast_address, unsigned int ssrc, int channels=1, bool quiet=false);

    /*!
     * \brief Read the sample rate as specified in the RTP stream
     */
    virtual int sample_rate() const = 0;

    /*!
     * \brief Return the number of bits per sample as specified in
     * the RTP stream.
     */
    virtual int bits_per_sample() const = 0;

    /*!
     * \brief Return the number of channels in the RTP stream as
     * specified in the RTP stream. This is also the max number
     * of outputs you can have.
     */
    virtual int channels() const = 0;

    /*!
     * Set SSRC
     *
     * \param ssrc new SSRC
     */
    virtual void set_ssrc(unsigned int ssrc) = 0;

    /*!
     * Get SSRC
     *
     * \return current SSRC
     */
    virtual unsigned int get_ssrc() const = 0;
};

} // namespace rtp
} // namespace gr

#endif /* INCLUDED_RTP_SOURCE_H */
