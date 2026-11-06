#pragma once

// =============================================================================
// AI Voice Morph (neural) - ARCHITECTURAL INTERFACE ONLY. NOT IMPLEMENTED.
// =============================================================================
//
// No class in this project implements this interface yet, and the UI shows the
// neural morph only as a roadmap item. See docs/ARCHITECTURE.md ("Motor neuronal").
//
// Design rules for any future implementation:
//  * Inference never runs on the audio thread. A worker thread (or separate
//    process) consumes audio from a lock-free FIFO and produces converted audio
//    into another lock-free FIFO.
//  * Studio mode (first): offline/non-real-time rendering of a selected region;
//    the plugin plays back the rendered result. Latency is irrelevant there.
//  * Live mode: only if measured worst-case inference time per hop on target
//    hardware is comfortably below the hop duration. Reported latency must
//    include model look-ahead + FIFO depth, fixed at prepare time.
//  * Fallback: if the worker misses a deadline, the audio thread outputs the
//    DSP path (or dry) with a short crossfade, never silence or garbage.
//  * Models: only with clear licences for code, weights, training data and the
//    voice owner's consent. See docs/TECH_EVALUATION.md.

#include <cstdint>
#include <string>

namespace vs
{

struct NeuralModelInfo
{
    std::string id;
    std::string displayName;
    std::string licence;       // must be verified before shipping
    std::string voiceConsent;  // reference to documented consent of the voice owner
    double nativeSampleRate = 0;
    int hopSamples = 0;
    int lookAheadSamples = 0;
};

class INeuralVoiceConverter
{
public:
    virtual ~INeuralVoiceConverter() = default;

    // Worker / message thread: loads model, may allocate and block.
    virtual bool loadModel (const std::string& path, NeuralModelInfo& infoOut, std::string& errorOut) = 0;

    // Worker thread only: converts one mono hop. Never called from processBlock.
    virtual bool convertHop (const float* in, float* out, int numSamples) = 0;

    virtual int latencySamples() const = 0;
};

} // namespace vs
