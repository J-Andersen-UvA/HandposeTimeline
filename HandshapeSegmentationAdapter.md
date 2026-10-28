# Handshape Segmentation Adapter

This Unreal Editor plugin samples local bone rotations from an Unreal animation asset, builds quaternion tracks, and runs the acceleration-based segmentation from `HandshapeSegmentation`.

Use the Blueprint node `Segment Animation` in an Editor Utility Blueprint. Supply:

- An animation asset.
- Pose evaluation options and an optional skeletal mesh.
- The finger bone names used for segmentation.
- The segmentation thresholds.

The returned result contains the sampled quaternion tracks, frame rate, frame count, segments, and an error message.

The adapter uses Unreal's animation sampling instead of the original FBX loading path. Quaternion smoothing, acceleration aggregation, and segment detection follow the colleague library's algorithm. The adapter aggregates every supplied quaternion track; the current standalone library contains an early `break` in its aggregation loop that otherwise limits it to the first track.
