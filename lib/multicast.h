// Multicast and RTP functions, constants and structures for ka9q-radio
// Copyright 2018-2023, Phil Karn, KA9Q

#ifndef _MULTICAST_H
#define _MULTICAST_H 1
#include <stdint.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netdb.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_MCAST_PORT (5004)
#define DEFAULT_RTP_PORT (5004)
#define DEFAULT_STAT_PORT (5006)

#define NTP_EPOCH 2208988800UL // Seconds between Jan 1 1900 and Jan 1 1970

#define RTP_MIN_SIZE 12  // min size of RTP header
#define RTP_VERS 2U
#define RTP_MARKER 0x80  // Marker flag in mpt field

// different PCM protocol types are used for flat audio and FM audio requiring de-emphasis
// De-emphasis is not done in the FM demodulators to make straight baseband FM available for digital demodulators
// Linear is flat, so it uses the standard PCM protocol types
// NBFM isn't standardized so it empirically uses a corner at 300 Hz (PL tones are below this) or 530.5 microsec
// WFM broadcast audio in the US formally requires de-emphasis with a 75 microsecond time constant
// WFM broadcast audio in Europe uses a 50 microsec time constant - not sure how to handle this
#define PCM_STEREO_PT (10)        // 48 kHz (or other) flat stereo baseband audio OR I/Q baseband audio OR I/Q IF stream
#define PCM_MONO_PT (11)          // 48 kHz (or other) flat mono baseband audio OR real-only IF stream
#define AIRSPY_PACKED (91) // Airspy packed format - eight 12 bit offset-2048 int samples in 3 32-bit ints
#define REAL_PT12 (92) // Non-standard payload types for real-only data
#define REAL_PT (93)
#define REAL_PT8 (94)
#define IQ_PT12 (95)  // NON-standard payload for 12-bit packed integers, BIG ENDIAN
#define AX25_PT (96)  // NON-standard payload type for my raw AX.25 frames
#define IQ_PT (97)    // NON-standard payload type for my raw I/Q stream - 16 bit little endian
#define IQ_PT8 (98)   // NON-standard payload type for my raw I/Q stream - 8 bit version
#define IQ_FLOAT (99) // 32-bit float complex

#define OPUS_PT (111) // Hard-coded NON-standard payload type for OPUS (should be dynamic with sdp)

#define PCM_MONO_LE_PT (114)      // 48 kHz (or other) single channel little endian
#define PCM_STEREO_LE_PT (115)    // 48 kHz (or other) dual channel little endian

#define PCM_MONO_24_PT (116)      // 24 kHz mono PCM, flat
#define PCM_STEREO_24_PT (117)    // 24 kHz stereo PCM, flat

#define PCM_MONO_16_PT (119)      // 16 kHz mono PCM, flat
#define PCM_STEREO_16_PT (120)    // 16 kHz stereo PCM, flat

#define PCM_MONO_12_PT (122)      // 12 kHz mono PCM, flat
#define PCM_STEREO_12_PT (123)    // 12 kHz stereo PCM, flat

#define PCM_MONO_8_PT (125)       // 8 kHz mono PCM, flat
#define PCM_STEREO_8_PT (126)     // 8 kHz stereo PCM, flat

extern int Mcast_ttl;
extern int IP_tos;


// Internal representation of RTP header -- NOT what's on wire!
struct rtp_header {
  int version;
  uint8_t type;
  uint16_t seq;
  uint32_t timestamp;
  uint32_t ssrc;
  bool marker:1;
  bool pad:1;
  bool extension:1;
  int cc;
  uint32_t csrc[15];
};

// RTP sender/receiver state
struct rtp_state {
  uint32_t ssrc;
  bool init;
  uint16_t seq;
  uint32_t timestamp;
  uint64_t packets;
  uint64_t bytes;
  uint64_t drops;
  uint64_t dupes;
};

// Internal format of sender report segment
struct rtcp_sr {
  unsigned int ssrc;
  int64_t ntp_timestamp;
  unsigned int rtp_timestamp;
  unsigned int packet_count;
  unsigned int byte_count;
};

// Internal format of receiver report segment
struct rtcp_rr {
  unsigned int ssrc;
  int lost_fract;
  int lost_packets;
  int highest_seq;
  int jitter;
  int lsr; // Last SR
  int dlsr; // Delay since last SR
};

// Internal format of RTCP source description
enum sdes_type {
  CNAME=1,
  NAME=2,
  EMAIL=3,
  PHONE=4,
  LOC=5,
  TOOL=6,
  NOTE=7,
  PRIV=8,
};

// Individual source description item
struct rtcp_sdes {
  enum sdes_type type;
  uint32_t ssrc;
  int mlen;
  char message[256];
};

char const *formatsock(void const *);
char *formataddr(char *result,int size,void const *s);

#define PKTSIZE 65536 // Largest possible IP datagram, in case we use jumbograms
// Incoming RTP packets
// This should probably be extracted into a more general RTP library
struct packet {
  struct packet *next;
  struct rtp_header rtp;
  uint8_t const *data; // Don't modify a packet through this pointer
  int len;
  uint8_t content[PKTSIZE];
};



// Convert between internal and wire representations of RTP header
void const *ntoh_rtp(struct rtp_header *,void const *);
void *hton_rtp(void *, struct rtp_header const *);

extern char const *Default_mcast_iface;

int setup_mcast(char const *target,struct sockaddr *,int output,int ttl,int tos,int offset);
static inline int setup_mcast_in(char const *target,struct sockaddr *sock,int offset){
  return setup_mcast(target,sock,0,0,0,offset);
}
int connect_mcast(void const *sock,char const *iface,int const ttl,int const tos);
int listen_mcast(void const *sock,char const *iface);
int resolve_mcast(char const *target,void *sock,int default_port,char *iface,int iface_len);
int setportnumber(void *sock,uint16_t port);
int getportnumber(void const *sock);
int address_match(void const *arg1,void const *arg2);

// Function to process incoming RTP packet headers
// Returns number of samples dropped or skipped by silence suppression, if any
int rtp_process(struct rtp_state *state,struct rtp_header const *rtp,int samples);

// Generate RTCP source description segment
uint8_t *gen_sdes(uint8_t *output,int bufsize,uint32_t ssrc,struct rtcp_sdes const *sdes,int sc);
// Generate RTCP bye segment
uint8_t *gen_bye(uint8_t *output,int bufsize,uint32_t const *ssrcs,int sc);
// Generate RTCP sender report segment
uint8_t *gen_sr(uint8_t *output,int bufsize,struct rtcp_sr const *sr,struct rtcp_rr const *rr,int rc);
// Generate RTCP receiver report segment
uint8_t *gen_rr(uint8_t *output,int bufsize,uint32_t ssrc,struct rtcp_rr const *rr,int rc);

void dump_interfaces(void);

// Utility routines for reading from, and writing integers to, network format in char buffers
static inline uint8_t get8(uint8_t const *dp){
  assert(dp != NULL);
  return *dp;
}

static inline uint16_t get16(uint8_t const *dp){
  assert(dp != NULL);
  return dp[0] << 8 | dp[1];
}

static inline uint32_t get24(uint8_t const *dp){
  assert(dp != NULL);
  return dp[0] << 16 | dp[1] << 8 | dp[2];
}

static inline uint32_t get32(uint8_t const *dp){
  assert(dp != NULL);
  return dp[0] << 24 | dp[1] << 16 | dp[2] << 8 | dp[3];
}

static inline uint8_t *put8(uint8_t *dp,uint8_t x){
  assert(dp != NULL);
  *dp++ = x;
  return dp;
}

static inline uint8_t *put16(uint8_t *dp,uint16_t x){
  assert(dp != NULL);
  *dp++ = x >> 8;
  *dp++ = x;
  return dp;
}

static inline uint8_t *put24(uint8_t *dp,uint32_t x){
  assert(dp != NULL);
  *dp++ = x >> 16;
  *dp++ = x >> 8;
  *dp++ = x;
  return dp;
}

static inline uint8_t *put32(uint8_t *dp,uint32_t x){
  assert(dp != NULL);
  *dp++ = x >> 24;
  *dp++ = x >> 16;
  *dp++ = x >> 8;
  *dp++ = x;
  return dp;
}
int samprate_from_pt(int type);
int channels_from_pt(int type);
int deemph_from_pt(int type);
char const *id_from_type(int type);
int pt_from_info(int samprate,int channels);

#ifdef __cplusplus
}
#endif

#endif
