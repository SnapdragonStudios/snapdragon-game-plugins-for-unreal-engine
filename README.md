# Snapdragon™ Game Plugins for Unreal Engine

### Table of contents
- [Snapdragon™ Game Plugins for Unreal Engine](#snapdragon-game-plugins-for-unreal-engine)
    - [Table of contents](#table-of-contents)
- [Introduction](#introduction)
- [Usage Instructions](#usage-instructions)
- [List of Plugins](#list-of-plugins)
  - [Snapdragon™ Game Super Resolution](#snapdragon-game-super-resolution)
- [License](#license)

# Introduction

This repository is a collection of plugins for the Unreal Engine, developed and authored by the Snapdragon™ Studios team.

This component is part of the [Snapdragon™ Game Toolkit](https://developer.qualcomm.com/gametoolkit).

# Usage Instructions

Unreal Engine contains multiple major versions, some released a few years ago but still used by many developers and game studios. Because of this, our repository is structured to provide plugins on a similar way:

- Select your major engine version in one of the branches in this repository
- Plugins are always contained in the "Plugins" directory
- Follow any extra instructions contained at the plugin of your choice

Note: The plugins are normally just drag and drop, and usually they can all be used as both an engine and project plugins, exceptions and extra instructions will be listed on the plugin readme, inside its own folder, if any.

# List of Plugins

## Snapdragon™ Game Super Resolution 

*Available Versions:*
| [4.27](https://github.com/quic/snapdragon-game-plugins-for-unreal-engine/tree/engine/4.27/Plugins/SGSR) | [5.0](https://github.com/quic/snapdragon-game-plugins-for-unreal-engine/tree/engine/5.0/Plugins/SGSR) | [5.1](https://github.com/quic/snapdragon-game-plugins-for-unreal-engine/tree/engine/5.1/Plugins/SGSR) | [5.2](https://github.com/quic/snapdragon-game-plugins-for-unreal-engine/tree/engine/5.2/Plugins/SGSR) | 
|------|-----|-----|-----|

Snapdragon™ Game Studios developed Snapdragon™ Game Super Resolution (Snapdragon™ GSR or SGSR), which integrates upscaling and sharpening in one single GPU shader pass. The algorithm uses a 12-tap Lanczos-like scaling filter and adaptive sharpening filter, which presents smooth images and sharp edges.

Our solution provides an efficient solution for games to draw 2D UI at device resolution for better visual quality, while rendering the 3D scene at a lower resolution for performance and power savings.

<img src="media/snapdragon_gsr_video.gif" width="500" height="500" />

The technique has visual quality on par with other spatial upscaling techniques while being highly optimized for Adreno™ GPU hardware.

More information can be found at https://github.com/quic/snapdragon-gsr

# License

Snapdragon™ Game Super Resolution is licensed under the BSD 3-clause “New” or “Revised” License. Check out the [LICENSE](LICENSE) for more details.