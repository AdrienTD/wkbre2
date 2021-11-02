# wkbre2

An open-source engine reimplementation for Warrior Kings (Battles), a Real-Time Strategy game
developed by Black Cactus and released in 2002/2003, now with multiplayer support!

This is an early development in progress. It's still not completely playable.

Currently works on Windows as well as on Ubuntu!

[Small demo video on YouTube:](https://youtu.be/WBQn7A8Gpwg)
[![wkbre2 Video Demo](https://img.youtube.com/vi/WBQn7A8Gpwg/0.jpg)](https://youtu.be/WBQn7A8Gpwg)


## Why?

Maybe you already know me and have heard about **wkbre**, which was my first attempt at making
an engine reimplementation of WK that I made a while ago.

It would open the game files and read the game assets (unit definitions, models, ...) to let you 
see a savegame and edit it. It also was able to let you play the game in it, even though it was
very limited, and was especially designed to only work in single player.

For wkbre I mainly focused on the "map editor" aspects, so that players could finally
make their own custom maps, as the original engine didn't really have a map editor (there
is a hidden map editor in the original game, but it was buggy and limited to terrain editing,
and the devs didn't have the opportunity to finish it).

Now, why a second engine reimplementation? The reason is because the code of the first wkbre
didn't age well. I made it around 6 years ago, I was only 16, back then I just
started programming in C++, as before I only did C, and that shows xD. There's a lot of bad code:
very short variable names, a lot of global variables and functions... :yuck:

Additionally, the original engine also starts to have its issues: random crashes on big games,
performance issues, pathfinding of units stop working, graphics issues on modern GPUs, and more...
And fans who work on mods on the game also realise its limitations, such as model and texture
size restrictions which stop them from implementing what they would like to see in their mods.

Thus, there is a need for a new engine replacement that could solve all issues the original engine has,
as well as add improvements such as QoL features and better mod support.

## How?

This project is an engine recreation. That means it basically tries to imitate the original engine
by loading the exact same asset and script files from the original.

WK uses a set of script files named the **game set**, which are files that define all the types of units
(characters, buildings) as well as the scripting for the game mechanics, commands and orders in the game.
What is nice about the game set is that, once extracted from the data.bcp archive file,
all the game set files are text-based, which makes reverse engineering and modding a lot easier. 

The goal of the new engine is to actually load the definitions and interpret the scripts
in the game set, similar to for example ScummVM which interprets scripting languages of several
adventure games, but here we have a 3D RTS game instead. this way the new engine already supports
nearly all mechanics of the game without having to reimplement them from scratch, saving
lots of time in making a "new" WK.

One catch though is that by reusing the game set, the new engine might become close to
the original engine. One might think this could bring back design flaws and bugs the original
had. However, I do believe that with few improvements to the design and game set, as well
as bugfix mods to the game set files, we could fix all of the issues and bring the game up to "modern standards".

## Architecture

For the original engine, as the game has to support multi-player, the devs knew that
multi-player had to be taken in consideration during early development if they wanted
a good network implementation, unlike lots of other games where multi-player is added at the end
of development which could cause lots of issues if the devs realize that what they have done so far
wasn't well designed for multi-player.

The devs opted for a **client-server architecture**. The game is actually split into
two separate programs: the **server** handles all the game logic processing and state of a game,
whereas the **client** is only a way for the player to see a *preview* of the server's game
(locations of units, buildings) and send *commands* to the server (issue orders to a unit).
Every time something changes in the game, like a unit spawned, the server will send a notification
to all clients describing what happened, and the clients register that to update the player's view.
Because the server is the only one who maintains the main state of the game, and that
all unit orders have to pass through the server, the integrity of the game is maintained
and cheating can be prevented by having the server check the client's order validity before
accepting it.

For the new engine, I decided to stick with the same architecture, since I believe the
current architecture is already good enough and I don't see better alternatives. One
disadvantage of client-server architecture is that it might need more network traffic,
but we need to consider that the game was released in 2003 and some people already managed
to play multiplayer back then, and I do hope that since then until 2021 people have better
Internet connections now. xD

Additionally, there is one advantage for single-player and the multi-player host:
As the server and client are separate programs, we can run them both at the same,
each having their own thread. This means that if you have a dual-core CPU, which are already
common since the mid 2000's, both server and client can run at the same time, each on their
own core, which could improve the game's performance significantly.
The server on one core would do all the game logic, pathfinding, ... and the client on
the other core would do the rendering of the scene. This means it is possible to keep a
good framerate even during local server lags, and no lags in game logic during heavy rendering.

For graphics rendering, wkbre2 supports several APIs: Direct3D 9, Direct3D 11
as well as OpenGL 3.3 (Core Profile). The rendering code is structured
such that each part of the rendering process (scene, terrain, particles) can
have different implementations: there is one "default" implementation that will
work on all APIs, but in the future we might have "special" implementations
that take advantages of the API's modern features to optimize performance
and image quality.

## Usage
 - [Download a build](https://github.com/AdrienTD/wkbre2/releases) and extract the zip.
 - Open the **wkconfig.json** file and change:
   - **game_path** to the location of the folder where the game is installed
     (where data.bcp is present, double black-slashes \\\\ are important!)
   - **gameVersion** to 1 if the located game is the first **Warrior Kings**,
     2 if it is the second **Warrior Kings Battles**.
 - Launch wkbre2

**NOTE**: If the program crashes right at the start without any error, ensure that
**wkconfig.json** doesn't have any syntax errors (missing commas, quotes, braces, ...).

## Compiling

### Windows

For compiling you need:
 - Visual Studio 2019 (2017 might still work, older versions weren't tested)
 - vcpkg with the following packages installed:
   - bzip2
   - discord-rpc
   - enet
   - glew
   - mpg123
   - nlohmann-json
   - openal-soft
   - sdl2
   - stb
 - Ensure that [vcpkg integration](https://docs.microsoft.com/en-us/cpp/build/integrate-vcpkg) is applied

Then you just need to open the root folder of this repo in Visual Studio as a CMake project,
then compile the project.

### Ubuntu

**NOTE**: Right now the building process is bad. I tried using CMake and vcpkg on Ubuntu but
had some trouble, and I'm not used to Linux. Thus for now there is a regular Makefile instead. I hope I can manage
to make the CMake files run on Ubuntu too in the future.

- Open a terminal.
- `cd \<repo root\>`
- `sudo apt-get install libbz2-dev libenet-dev libglew-dev libmpg123-dev nlohmann-json3-dev libopenal-dev libsdl2-dev`
- `mkdir inc`
- Download the [stb headers](https://github.com/nothings/stb) and copy them to the inc folder
- `cd wkbre2`
- `make -f ../Makefile`
