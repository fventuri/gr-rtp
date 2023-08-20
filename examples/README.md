# Examples

Some simple examples on how to use the RTS source block to connect to ka9q-radio 'radiod' program


IMPORTANT: before running GNU Radio with this RTP source block you should make sure that ka9q-radio is streaming on the requested SSRC. An easy way to check this is to use the 'monitor' program; for instance in the case of the NOAA example below:
```
monitor nws-pcm.local
```
You should see an SSRC=162550, and you should hear 'something' (actual voice or static, depending if there's a NWS station broadcasting on 162.550MHz or not).
If you don't see any SSRC when running 'monitor' or you don't hear any sound in your default audio device, you'll need to troubleshoot your ka9q-radio configuration before going ahead.


## NOAA radio FM
This example uses ka9q-radio to demodulate the local NOAA weather station on 162.550MHz (FM).


Below is the ka9q-radio radiod configuration file I used for this example.

A couple of important notes:
- the 'input' parameter will need to be changed based on your front-end receiver in ka9q-radio
- recently (July/August 2023) the architecture of ka9q-radio radiod has changed and it now includes most of the front-end receivers; because of this, the format of its configuration file has changed; please refer to the examples distributed with ka9q-radio.

```
# Standard national weather service broadcasts
[global]
input = rspdx-antC.local
iface = lo
samprate = 24000
mode = fm
status = nws.local

[NWS]
data = nws-pcm.local
freq = "162m400 162m425 162m450 162m475 162m500 162m525 162m550"'
```


## WSPR USB
This example uses ka9q-radio to down convert WSPR on 20m (14,095.6kHz USB).


Below is the ka9q-radio radiod configuration file I used for this example.

See te ka9q-radio notes above.

```
# WSPR on HF bands
[global]
input = rspdx-antC.local
iface = lo
samprate = 12000
mode = usb
status = wspr.local

[WSPR]
data = wspr-pcm.local
freq = "136k000 474k200 1m836600 3m592600 5m287200 7m038600 10m138700 14m095600 18m104600 21m094600 24m924600 28m124600"
```
