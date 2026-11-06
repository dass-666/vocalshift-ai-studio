#pragma once
// Reference output of the dec340d engine (macros path, mono 48 kHz, 512-sample blocks):
// RMS in dB for consecutive 20 ms windows of a synthetic phrase. Generated once from the
// verified build, used to prove that the legacy path is unchanged.
namespace golden
{
inline constexpr float golden0[] = {-120.00f,-120.00f,-120.00f,-77.21f,-70.99f,-61.09f,-50.49f,-20.20f,-17.29f,-17.99f,-18.22f,-18.70f,-19.00f,-18.78f,-18.48f,-18.42f,-19.31f,-19.87f,-20.02f,-20.36f,-20.12f,-20.40f,-19.81f,-19.67f,-19.39f,-19.11f,-19.34f,-23.55f,-46.53f,-59.96f,-72.90f,-78.63f,-88.10f,-87.81f,-76.65f,-67.46f,-52.68f,-29.97f,-25.56f,-26.94f,-26.94f,-26.98f,-22.70f,-20.12f,-20.09f,-20.27f,-20.87f,-21.43f,-21.25f,-21.10f,-21.43f,-21.21f,-20.93f,-20.95f,-20.95f,-20.53f,-21.10f,-23.66f,-46.97f,-55.36f,};
inline constexpr float golden1[] = {-120.00f,-120.00f,-120.00f,-82.25f,-80.18f,-68.37f,-56.21f,-25.38f,-21.94f,-23.13f,-23.48f,-23.95f,-24.16f,-24.02f,-23.92f,-23.94f,-24.39f,-24.76f,-24.77f,-25.15f,-25.01f,-25.13f,-24.93f,-24.46f,-24.40f,-24.39f,-24.54f,-28.32f,-54.29f,-67.23f,-81.28f,-87.02f,-95.20f,-92.25f,-80.50f,-71.51f,-56.52f,-33.47f,-28.89f,-29.94f,-29.99f,-30.18f,-27.36f,-24.91f,-25.86f,-25.68f,-26.24f,-26.67f,-26.43f,-25.75f,-26.26f,-26.53f,-26.02f,-26.12f,-26.23f,-25.74f,-26.43f,-28.88f,-52.50f,-62.33f,};
} // namespace golden
