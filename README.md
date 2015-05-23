# Speckled Bushcricket Wireless Sensor Network
Wireless Sensor Network for Acoustic Monitoring of Speckled Bush Crickets

##Known Issues (Node)
* Assembled sensor node temperature and light level sensors are broken (heat damage possibly)
* Sensor node environment sensors will require creative mounting for light and RH to be exposed outside. Suggest modifying onto an external header to another board (would also solve above problem)

##Known Issues (Base)
* GSM Modem doesn't currently execute HTTP requests, an error returns after setting the URL
* Base unit sometimes doesn't wake up at same time as node. Fitting a crystal to the Discovery board and switching RTC to the external 32k oscillator may help
* Power supplies start and generate stable output, however destroy regulator ICs after Discovery board is connected. Recommend thorough design review
* Discovery board will not fit unless headphone connector and button caps are removed. Suggest reworking to flip board, so Discovery fits on top and modem on bottom

