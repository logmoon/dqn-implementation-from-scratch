# dqn-implementation-from-scratch

A Deep Q-Network implemented from scratch in C that learns to play a Pong-like game. The network, backpropagation, replay buffer, target network, and epsilon-greedy policy live in [`src/rl.c`](src/rl.c); rendering, audio, and input are handled by [sokol](https://github.com/floooh/sokol), with a custom build system based on [nob](https://github.com/tsoding/nob.h).

**Important Note:**
This was a little experiment I did over a year ago (from the time this repo is published) from my current testing the model doesn't converge to a good policy anymore, and that's due to a bunch of tweaks I did that I don't want to bother hunying down/fixing right now, this is merely about the implementation of the DQN algorithm and the neural network math, so if you take anything out of this repo, it's that.

## Build

Requires GCC and the usual X11/OpenGL/ALSA dev packages on Linux, also should work on Windows

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
