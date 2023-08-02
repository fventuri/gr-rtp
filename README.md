# gr-rtp: GNU Radio OOT module for RTP stream sources

This module contains a source block that reads from an RTP stream (identified by its multicast address and SSRC) and can output the data in several formats: complex (suitable for I/Q streams), interleaved shorts (suitable for I/Q streams), float with one channeli (mono), float with two channels (stereo), short with one channel (mono), and short with two channels (stereo).

The immediate purpose of this module is to allow to send the multicast stream(s) from ka9q-radio (https://github.com/ka9q/ka9q-radio) 'radiod' into GNU Radio for further processing. The core of this OOT module uses the code from pcmcat.c (RTP session management) and multicast.c (multicasting) from ka9q-radio.

The module might work with other RTP sources but it hasn't been tested (yet) outside of ka9q-radio radiod as the source.


## Build and installation

```
cd gr-rtp
mkdir build && cd build && cmake ../ && make
sudo make install
```


## Credits

- Phil Karn, KA9Q for the program ka9q-radio and the idea of using a fast convolution filter bank to tune into hundreds of different channels at the same time (see ka9q-radio documentation: https://github.com/ka9q/ka9q-radio/tree/main/docs)


## License

GPLv3
