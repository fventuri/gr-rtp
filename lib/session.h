// from ka9q-radio pcmcat.c
// Copyright 2023, Phil Karn, KA9Q

#ifndef _SESSION_H
#define _SESSION_H 1

#include <stdint.h>
#include <sys/socket.h>
#include <netdb.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pcmstream {
  struct pcmstream *prev;       // Linked list pointers
  struct pcmstream *next;
  uint32_t ssrc;            // RTP Sending Source ID
  int type;                 // RTP type (10,11,20)

  struct sockaddr sender;
  char addr[NI_MAXHOST];    // RTP Sender IP address
  char port[NI_MAXSERV];    // RTP Sender source port

  struct rtp_state rtp_state;
};

struct pcmstream *lookup_session(const struct sockaddr *sender, const uint32_t ssrc, struct pcmstream **Pcmstream_p);
struct pcmstream *make_session(struct sockaddr const *sender, uint32_t ssrc, uint16_t seq, uint32_t timestamp, struct pcmstream **Pcmstream_p);
int close_session(struct pcmstream *sp, struct pcmstream **Pcmstream_p);

#ifdef __cplusplus
}
#endif

#endif
