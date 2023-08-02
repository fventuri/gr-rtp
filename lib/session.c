// Session functions from from ka9q-radio pcmcat.c
// Copyright 2023 Phil Karn, KA9Q

// this must come first - fv
#define _GNU_SOURCE

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "multicast.h"
#include "session.h"

// How the free() library routine should have been all along: null the pointer after freeing!
#define FREE(p) (free(p), p = NULL)

struct pcmstream *lookup_session(const struct sockaddr *sender,const uint32_t ssrc, struct pcmstream **Pcmstream_p){
  struct pcmstream *sp;
  for(sp = *Pcmstream_p; sp != NULL; sp = sp->next){
    if(sp->ssrc == ssrc && address_match(&sp->sender,sender)){
      // Found it
      if(sp->prev != NULL){
	// Not at top of bucket chain; move it there
	if(sp->next != NULL)
	  sp->next->prev = sp->prev;

	sp->prev->next = sp->next;
	sp->prev = NULL;
	sp->next = *Pcmstream_p;
	*Pcmstream_p = sp;
      }
      return sp;
    }
  }
  return NULL;
}

// Create a new session, partly initialize
struct pcmstream *make_session(struct sockaddr const *sender,uint32_t ssrc,uint16_t seq,uint32_t timestamp, struct pcmstream **Pcmstream_p){
  struct pcmstream *sp;

  if((sp = calloc(1,sizeof(*sp))) == NULL)
    return NULL; // Shouldn't happen on modern machines!
  
  // Initialize entry
  memcpy(&sp->sender,sender,sizeof(struct sockaddr));
  sp->ssrc = ssrc;

  // Put at head of bucket chain
  sp->next = *Pcmstream_p;
  if(sp->next != NULL)
    sp->next->prev = sp;
  *Pcmstream_p = sp;
  return sp;
}

int close_session(struct pcmstream *sp, struct pcmstream **Pcmstream_p){
  if(sp == NULL)
    return -1;
  
  // Remove from linked list
  if(sp->next != NULL)
    sp->next->prev = sp->prev;
  if(sp->prev != NULL)
    sp->prev->next = sp->next;
  else
    *Pcmstream_p = sp->next;
  FREE(sp);
  return 0;
}
