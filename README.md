# Arduino Battery Saver / Monitor

This sketch controls some outputs depending on the detected battery voltage and light levels.
(These outputs can then trigger relays or MOSFETs)

One output is triggered if voltage is good. (ie. battery isn't going flat)

The other output is triggered if voltage is good AND we think it is dark outside.

The light level part is optional, but useful if you are controlling lighting which you want to turn
off automatically in daytime.

## Note on suitable relays

Ensure you're using relays which don't need high currents to work.
There are plenty of little boards which are advertised as Arduino compatible, which will have 5V
relay drivers. For instance, http://wiki.netduino.com/SainSmart-5V-Relay-Module.ashx

## Note on voltage divider

I designed this thing to work with 12V battery systems, which could reach 15V during charging.
Because the Arduino ADC can only work with 5V signals, you need to implement a voltage divider with
a couple of resistors. The code, as it stands, assumes the voltage is divided into a third.

## Note on power saving modes

The code here doesn't continually check the power; instead it sleeps for a few seconds between
checks. It attempts to drop into a power-saving mode whilst sleeping. You'll notice that the
code includes two methods of doing this - however the sleep mode is not used, as it just seemed
to crash my board and I haven't debugged it. The other mode does seem to reduce power and sleep,
but doesn't work quite as I expected.

