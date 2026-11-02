# Vespera Material Mood Board

<!-- One sentence in your own words: what it is and what it does. -->

![Final prototype](docs/photos/FINAL_PHOTO.jpg)

*CASA0014 Connected Environments · UCL · Arduino MKR WiFi 1010 → MQTT → Vespera*

## Concept
<!-- Your words: the idea, who it is for (e.g. a showroom / hotel lobby), why lighting matters for materials. -->

## Inspiration
<!-- Your words: interior material boards, references (link and credit them). -->

**Diagrams:** [Concept](docs/diagrams/concept.jpg) · [System](docs/diagrams/system_diagram.png) · [Wiring](docs/diagrams/wiring_diagram.png)

## How it works
1. Six material tiles float on hidden push buttons.
2. When nothing is pressed, Vespera shows the board's palette (two columns per material).
3. Pressing a tile shows that material's light scene.
4. After 60 seconds with no press, Vespera returns to the palette.

The MKR1010 builds a 72-LED frame (216 bytes of RGB) and publishes it to `student/CASA0014/luminaire/15` on `mqtt.cetools.org`, only when the frame changes.

![System diagram](docs/diagrams/system_diagram.png)

## Hardware
| Component | Purpose |
|-----------|---------|
| Arduino MKR WiFi 1010 | Reads the buttons, publishes to MQTT over Wi-Fi |
| 6 tactile push buttons | One under each interactive material tile |
| Hook-up wire, solder, heat-shrink, hot glue | Soldered button connections |
| White foam board (4 × A4), cardboard, craft sticks | Base, spacers and tile posts |
| Material samples | Fluted wood, grey oak, blue glass, oak, silver mosaic, fabric + decorative pieces |
| USB power bank | Portable power |

## Wiring
| Tile | Material | MKR pin | Wire colour |
|------|----------|---------|-------------|
| 1 | Fluted wood | D1 | yellow |
| 2 | Grey oak | D2 | orange |
| 3 | Blue glass | D3 | blue |
| 4 | Oak block | D4 | purple |
| 5 | Silver mosaic | D5 | red |
| 6 | Fabric | D0 | grey |
| — | GND chain | GND | green / black |

- Each button: one leg → its pin, the **diagonal** leg → GND (legs on the same side are joined inside the button).
- `INPUT_PULLUP`, so no resistors are needed.
- Pin 6 is avoided: it drives the MKR1010's built-in LED.

![Wiring diagram](docs/diagrams/wiring_diagram.png)

<!-- Check the wire colours match what you actually soldered. -->

## Iterations
| Version | What changed |
|---------|--------------|
| FACE ID idea | <!-- your words: why you moved away from face recognition --> |
| [v3](sketches/v3_pond_controller/) | Paper pond, 5 buttons, moving boat stripes |
| [v4](sketches/v4_pond_controller/) | 6th button, two-tone colours |
| Soldering | Moved from breadboard to soldered buttons |
| [v5](sketches/v5_mood_board/) | Material mood board, idle palette, 60 s timeout, moving mosaic |

## Testing
See [tests/](tests/) and the [devlog](docs/devlog.md).
<!-- Your words: the diagonal-leg bug and how you found it; what the colours looked like on the real Vespera vs Lumi. -->

## Reflection and future work
<!-- Your words: what worked, what was hard, what you'd do next. -->

## Getting started
1. Open `sketches/v5_mood_board/v5_mood_board.ino` in the Arduino IDE and select **Arduino MKR WiFi 1010**.
2. Install the libraries **WiFiNINA** and **PubSubClient**.
3. Copy `arduino_secrets.example.h` to `arduino_secrets.h` in the same folder and fill in your Wi-Fi and MQTT details. Never commit it.
4. Change `lightId` to your own Vespera number.
5. Wire the buttons as above and upload.

## Repository structure
| Folder | Contents |
|--------|----------|
| [sketches/](sketches/) | v3, v4 and v5 Arduino sketches |
| [tests/](tests/) | Test sketches and results |
| [docs/diagrams/](docs/diagrams/) | Concept, system and wiring diagrams |
| [docs/photos/](docs/photos/) | Build and prototype photos |
| [docs/devlog.md](docs/devlog.md) | Dated development log |

## AI use
**Claude (Anthropic)** – code drafting and debugging, diagrams, documentation structure 
**What AI helped with**
- Drafted the sketch framework: button debouncing, non-blocking frame timer,
 the Boat/oak stripe animation, and the v5 structure
  (idle palette, 60 s timeout, moving mosaic)
- Generated the system and wiring diagrams in docs/diagrams/
- Suggested the README structure and starting RGB values
  
**What I did**
- Concept and pivots (FACE ID → pond → material mood board) and the material selection
- All wiring, soldering and the physical build
- Found the diagonal-leg button fault through testing
- Tested colours on Lumi and the real Vespera and chose the final values
- Wrote the concept, reflection and devlog

**Example prompts**
- "Help me wire 5 push buttons to the MKR1010 and send colours to Vespera"
- "Check my code, the first button works but the second doesn't"
- "Change the colours to match my moodboard and add an idle palette"
- "Make a system diagram and circuit diagram for my material moodboard project"

## Future use
Room-scale ambient lighting that responds to the materials being viewed
Real colour temperature (2700K–6500K) using tunable-white LEDs, since RGB LEDs can’t produce warm white or grey
A showroom version: scan or tap any sample to see it under its intended lighting
Logging which materials visitors press, so designers learn what people are drawn to

## Licence
MIT
