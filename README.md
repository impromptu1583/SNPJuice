![Screenshot](Screenshot%202024-06-06%20091103.png?raw=true "Screenshot")


### CrownLink
Crownlink is a networking provider that is compatible with blizzard's storm-based games from the 90s. It's specifically developed with the starcraft overhaul [Cosmonarchy](https://fraudsclub.com/cosmonarchy-bw/) in mind, but should work for other blizzard games from the era.

## Usage
Download the latest `CrownLink.snp` from releases and drop it in your game directory next to the default ones. For Cosmonarchy this is typically `C:\Cosmonarchy\Starcraft\`. You should have a new entry in the multiplayer menu show up.

### IMPORTANT:
Please make sure you have an up to date version of microsoft visual c++ redistributable x86 installed:
[vc_redist.x86.exe](https://aka.ms/vs/17/release/vc_redist.x86.exe)

(comes from this site: [learn.microsoft.com](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-140))

## Connection modes / tags
You may see games in the multiplayer menu tagged with `[Relayed]` in the game name. This means direct peer to peer communication couldn't be established and a TURN relay server is in use. Performance may not be as good as a direct peer to peer connection.

# License
This version of the code has some files from [BWAPI](https://github.com/bwapi/bwapi) and is therefore licensed GPLv3. (TODO: update licenses file)
