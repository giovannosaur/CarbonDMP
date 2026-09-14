# CarbonDMP (Carbon Dynamic Music Player)

CarbonDMP is a dynamic external music player mod built for Need for Speed: Carbon.

---

## Features

- Context-aware music switching: you can configure which songs to be played on FE (Main Menu, Safehouse, Car Lot), Gameplay (Freeroam and Race), or both.
- Automatic music pause/stop on certain game states (e.g. loading screens, FMVs, non-interactive cutscenes)
- Speed-Sensitive Audio: reads your vehicle speed while in Race or Freeroam; then adjusts LPF + Volume levels based on it. (basically, faster = louder and clearer)
- Interior Audio On Idle: detects if vehicle has been on idle for around 10 seconds while in Race or Freeroam; applies an "interior car speaker" effect chain when detected.
- Speedbreaker LPF: applies low pass filter effect on every game speedbreaker moments.
- Mini player overlay, with 2 different styles: Fancy and Simple.
- Music player UI with track, artist, and album information.
- In-game music player controls: Play/Pause, Next, Previous, Shuffle and Mute.
- Configurable keyboard controls from the CarbonDMP.ini configuration file (live editing currently not supported).
- Configurable audio settings.

---

## Build Requirements

The following dependencies are required to build:

- Microsoft Visual Studio
- DirectX 9 SDK
- BASS Audio Library
- BASS_FX
- MinHook

---

## External Dependencies

The project uses third-party libraries including:

- BASS Audio Library
- BASS_FX
- MinHook

Third-party components remain subject to their respective licenses and copyrights.

---

## Controls

Default keyboard controls:

- `7` - Toggle Music Player
- `I` / `K` - Navigate Up / Down
- `J` / `L` - Adjust Left / Right
- `U` - Back
- `O` - Enter
- `8` - Previous Track
- `9` - Play / Pause
- `0` - Next Track

The UI toggle key and other controls can be configured through CarbonDMP.ini

---

## Credits

- TsudaKageyu - MinHook
- Un4seen Developments Ltd. - BASS Audio Library + BASS_FX
- berkayylmao - NFSPluginSDK (game memory references)
- Zolika1351 - NFSC-SDK (game memory references)
- ARCHIE - NFSC_CustomHUD (game memory references)

---

## Development Note

CarbonDMP was developed with AI-assisted coding.

The mod's implementation, code and overall behavior itself was manually tested, debugged, reviewed, and adjusted by the author to run without any issues (experience-related or performance-related).

All visual assets (UI textures, etc.) and other components were created by the author without any AI generation involced.

---

## Disclaimer

This project is not affiliated with or endorsed by EA Games.

Need for Speed: Carbon is a property of Electronic Arts.
