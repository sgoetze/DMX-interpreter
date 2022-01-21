# DMX-interpreter
ESP32 project replaces a 192 CH DMX controller hardware

This is a replacement for a 192 CH DMX console used to control small lighting systems via DMX. Since the use of the DMX console is divided into the step-by-step control of 12 x 16 DMX channels, it is not as convenient to control strips of RGB neopixel LEDs. The pixels, each controlled by three DMX channels, do not fit so well with the 12 x 16 channel structure of the DMX console.

This solution uses an ESP32 to implement a web interface. In a web browser, a simple language with only a few commands can be used to program scenes and sequences. The written programme can be interpreted immediately and a connected MAX485 circuit outputs the DMX packets to the lighting system.

I used Arduino IDE and ESP32 Plugin.

Thanks to Mitch Weisbrod
(https://github.com/someweisguy/esp_dmx)
where I took the DMX control completely and unchanged.

And thanks to Rui and Sara Santos (Random Nerd Tutorials) for many tips and tricks around ESP8266 and ESP32.
 
