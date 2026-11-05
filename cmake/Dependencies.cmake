# Pinned third-party dependencies. Every entry here was checked for licence
# compatibility (see docs/TECH_EVALUATION.md). Do not add a dependency without
# updating that document.
#
# Offline / cached builds: point FETCHCONTENT_SOURCE_DIR_<NAME> at a local
# checkout, e.g. -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# JUCE 9.0.4 (AGPLv3 / JUCE 9 EULA, Starter tier free up to 20k USD revenue).
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.4 # commit 3fb5cbc0340758596dac39f7c8de47d562b4896e
    GIT_SHALLOW TRUE
)

# Signalsmith Linear 0.6.6 (MIT) - FFT/STFT used by Signalsmith Stretch.
FetchContent_Declare(signalsmith-linear
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/linear.git
    GIT_TAG 7bb1c8f36794d6980080ce817c91f8da8fe56a66 # 0.6.6
)

# Signalsmith Stretch 1.3.2 (MIT) - initial pitch/formant engine candidate.
FetchContent_Declare(signalsmith-stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG 88061fd4eda56a2353c5d65c74aa3f4ccf76f9da # main @ 2026-10-22, version 1.3.2
)

FetchContent_MakeAvailable(JUCE signalsmith-linear signalsmith-stretch)
