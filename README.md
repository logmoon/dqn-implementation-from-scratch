# dqn-implementation-from-scratch

A Deep Q-Network implemented from scratch in C that learns to play a Pong-like game. The network, backpropagation, replay buffer, target network, and epsilon-greedy policy live in [`src/rl.c`](src/rl.c); rendering, audio, and input are handled by [sokol](https://github.com/floooh/sokol), with a custom build system based on [nob](https://github.com/tsoding/nob.h).

## Build

Requires GCC and the usual X11/OpenGL/ALSA dev packages on Linux.

```console
cc -o nob nob.c -lm
./nob
```

Run it:

```console
./build/log_frame
```

## Controls

| Key | Action |
| --- | --- |
| `T` | Toggle training / play mode |
| `K` | Save the model to `pong_model.bin` |
| `L` | Load the model from `pong_model.bin` |
| `W` / `S` | Move your paddle (play mode) |
