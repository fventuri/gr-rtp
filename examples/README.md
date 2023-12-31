# Examples

Some simple examples on how to use the RTS source block to connect to ka9q-radio 'radiod' program


IMPORTANT: before running GNU Radio with this RTP source block you should make sure that ka9q-radio is streaming on the requested SSRC. An easy way to check this is to use the 'monitor' program; for instance in the case of the NOAA example below:
```
monitor nws-pcm.local
```
You should see an SSRC=162550, and you should hear 'something' (actual voice or static, depending if there's a NWS station broadcasting on 162.550MHz or not).
If you don't see any SSRC when running 'monitor' or you don't hear any sound in your default audio device, you'll need to troubleshoot your ka9q-radio configuration before going ahead.

If the test with 'monitor' is successful, you should be able to use the 'RTP source' block in a GNU Radio flowgraph.
When the flograph is run, the RTP source block will try to connect to the selected RTP stream (defined by its multicast address and SSRC), and if successful it will display on the terminal a message like this:
```
rtp_source :info: New session from 162550@localhost:59817, type 122, channels 1, samprate 12000
```


## NOAA radio FM
This example uses ka9q-radio to demodulate the local NOAA weather station on 162.550MHz (FM).


Below is the ka9q-radio radiod configuration file I used for this example.

Note that the SDR hardware configuration section will likely have to be changed to match your hardware SDR.

```
# Standard national weather service broadcasts
[global]
status = nws.local
hardware = sdrplay

[sdrplay]
device = sdrplay
description = RSPdx - Antenna C
#serial =
antenna = Antenna C
#samprate = 2000000        ; default is 2M
lna-state = 2
if-agc = yes

frequency = 162480000

[NWS]
mode = fm
data = nws-pcm.local
freq = "162m400 162m425 162m450 162m475 162m500 162m525 162m550"
```


## WSPR USB
This example uses ka9q-radio to down convert WSPR on 20m (14,095.6kHz USB).


Below is the ka9q-radio radiod configuration file I used for this example.

Note that the SDR hardware configuration section will likely have to be changed to match your hardware SDR.

```
# WSPR on HF bands
[global]
status = wspr.local
hardware = sdrplay

[sdrplay]
device = sdrplay
description = RSPdx - Antenna C
#serial =
antenna = Antenna C
#samprate = 2000000        ; default is 2M
lna-state = 2
if-agc = yes

frequency = 14100000

[WSPR]
mode = usb
data = wspr-pcm.local
freq = "136k000 474k200 1m836600 3m592600 5m287200 7m038600 10m138700 14m095600 18m104600 21m094600 24m924600 28m124600"
```
