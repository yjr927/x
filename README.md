# Tangible Earth

[打开装置控制台 · Live control panel](https://yjr927.github.io/x/)

Download this repository and open `TangibleEarth/TangibleEarth.ino` in Arduino IDE. Select **Arduino Uno** and upload. The sketch uses the Arduino **Servo** library, the bundled AVR Wire and EEPROM libraries, and the two headers beside the sketch.

To repeat the polar bear hardware test, open [PolarBearBench/PolarBearBench.ino](PolarBearBench/PolarBearBench.ino) instead. This sketch sets the display to 88°F on a new polar bear detection (ID 2) and moves the servo by 180°. Reset restores 70°F and the 180° home position.

Open `index.html` in desktop Chrome or Edge, close Arduino IDE's Serial Monitor, and click **连接 Arduino**. Select the Uno USB port. The web page works offline and sends commands directly over USB.

In the exhibit sketch, WonderCam Feature Learning IDs have these effects.

| ID | Learned object | Display | Servo |
| --- | --- | --- | --- |
| 1 | Penguin | Hold | Hold |
| 2 | Polar bear | Hold | Hold |
| 3 | Bicycle | Hold | Hold |
| 4 | Car | +20°F | One 180° stroke |
| 5 | Hand rake | Hold | Hold |
| 6 | Electric leaf blower | +20°F | One 180° stroke |

Present a car or electric leaf blower for at least 0.5 seconds. Each new detection moves the servo to the opposite endpoint (180° or 0°). A held object triggers once. Switching directly from car to blower or back triggers each object separately. To repeat the same object, show an empty view or a neutral object for at least one second first. New detections during motion queue consecutive strokes. With **仅手动**, the camera still reports matches while the operator controls the outputs independently. **回原位** moves only the motor. **Reset** sets the temperature to 70°F and the motor target to 180°, clearing queued strokes. The servo starts at 180° on power-up. The board remembers its last temperature and mode across USB reconnection.
