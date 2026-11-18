# Tangible Earth

[打开装置控制台 · Live control panel](https://yjr927.github.io/x/)

Download this repository and open `TangibleEarth/TangibleEarth.ino` in Arduino IDE. Select **Arduino Uno** and upload. The sketch uses the Arduino **Servo** library, the bundled AVR Wire and EEPROM libraries, and the two headers beside the sketch.

Open `index.html` in desktop Chrome or Edge, close Arduino IDE's Serial Monitor, and click **连接 Arduino**. Select the Uno USB port. The web page works offline and sends commands directly over USB.

WonderCam Feature Learning IDs are 1 penguin, 2 polar bear, 3 car, and 4 bicycle. In camera mode a stable car adds 20°F and moves the D6 servo to 180°. A held car triggers once. With **仅手动**, the camera still reports matches while the operator controls the outputs independently. **回原位** moves only the motor. **Reset** sets the temperature to 70°F and the motor target to 0°. The board remembers its last temperature, target, and mode across USB reconnection.
