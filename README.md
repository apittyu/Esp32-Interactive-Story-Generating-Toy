# Esp32-Interactive-Story-Generating-Toy
This project is aimed at creating a hardware and software for a toy that generates interactive stories from different time periods.

![Image](https://github.com/user-attachments/assets/d424a26b-8895-47dc-ad96-c3b44f032add)
Hardware:
- ESP32 C3 Super Mini,
- Rotary encoder, 
- 2x buttons,
- RGB LED,
- White LED

On startup the ESP32 connects to the Gemini API through WiFi. The user can select a year with the rotary encoder and generate a short story with the encoders built in button.
The story ends with two choices for the two buttons on the circuit so that the user can choose how the story continues. After the user making the choice the Gemini Api is again used to generate the
new part of the story with two new choices. The rgb led is for the indication of the selecction of a new year for a new story and for indication of choice "A", while the white led is for choice "B".

To use this code you need to set your own WIFI ssid and password and your own Gemini API, that you can get from: https://ai.google.dev/gemini-api/docs 

This is a work-in-progress project so it still has some issues that need to be adressed, meaning the story generating prompts need refining and the output of the stories is yet to be implemented so it only deposits the stories via the serial monitor
The capacitors and other circuit components not mentioned on the hardware list are not used by the circuit at the moment.
Ideas for further development:
- Composite video output displaying the story and the possible choices for the user.
- Ai generated Text-to-speech audio through a 6.3mm jack or speaker with integrated sound amplification or an audio module.
- A button for generating a short summary of the story so far.
- Case for the hardware
- 4 number - 7 segment display for indicating the year selected.
