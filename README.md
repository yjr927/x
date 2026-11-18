# Tangible Earth

[打开装置控制台 · Live control panel](https://yjr927.github.io/x/)

Download this repository and open `TangibleEarth/TangibleEarth.ino` in Arduino IDE. Select **Arduino Uno** and upload. The sketch uses the Arduino **Servo** library, the bundled AVR Wire and EEPROM libraries, and the two headers beside the sketch.

To repeat the polar bear hardware test, open [PolarBearBench/PolarBearBench.ino](PolarBearBench/PolarBearBench.ino) instead. This sketch sets the display to 88°F on a new polar bear detection (ID 2) and moves the servo by 180°. Reset restores 70°F and the 180° home position.

Open `index.html` in desktop Chrome or Edge, close Arduino IDE's Serial Monitor, and click **连接 Arduino**. Select the Uno USB port. The web page works offline and sends commands directly over USB.

WonderCam Feature Learning IDs are 1 penguin, 2 polar bear, 3 car, and 4 bicycle. In camera mode a stable car adds 20°F and moves the D6 servo one full 180° stroke, alternating between 180° and 0°. A held car triggers once. Give the camera a clear view without the car for at least one second before presenting it again. New detections during motion queue consecutive strokes. With **仅手动**, the camera still reports matches while the operator controls the outputs independently. **回原位** moves only the motor. **Reset** sets the temperature to 70°F and the motor target to 180°, clearing queued strokes. The servo starts at 180° on power-up. The board remembers its last temperature and mode across USB reconnection.
