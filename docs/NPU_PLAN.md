# NPU Plan (MCXN947)

## Reality Check
The NPU accelerates neural-network inference. It is not a generic compute accelerator.

Best early use: run a small fixed-weight CNN as a post-process effect on the rendered sim.

## Phase 1 (No Training Required): Visual Post-Processing
Run inference on a low-res indexed or grayscale buffer (ex: `160x120`):
- edge/glow for metal
- water shimmer/blur
- sand dithering/stylization
- motion trails

This can be implemented as a tiny convolution stack with fixed weights.

## Phase 2 (Optional): Surrogate Physics
Train offline (PC) a small model that predicts the next state of a local neighborhood:
- input: neighborhood (one-hot materials) + gravity vector
- output: next material or next patch

Deploy to NPU for higher tick-rate or more complex behaviors.

## Phase 3 (Optional): Gesture/UX Classifier
Use accelerometer time-series classification (shake/tap/swirl) to trigger game events.

## Integration Strategy
- CPU core 0: deterministic sim + display driver
- CPU core 1: NPU jobs / post-process

