# OBS Screen Capture Node Bindings

This package provides basic Node.js bindings to `libobs` from OBS Studio.
It exposes helper functions for checking macOS screen recording permission
and initializing/shutting down the OBS core. Only a few functions are
implemented as a proof of concept.

## Building

```
npm install
```

`libobs` must be built and available on your system. On Linux you can build
it from this repository using CMake. On macOS you can use the Xcode project
or CMake as well. Ensure `libobs` is discoverable by the linker.

## Testing

```
npm test
```

This will run `test/test-linux.js` on Linux systems. macOS users can run
`node test/test-macos.js`.

## API

```js
const obs = require('obs-screen-capture');

obs.checkScreenPermission();    // macOS only
obs.requestScreenPermission();  // macOS only

obs.init();                     // start OBS core
obs.startRecording('/tmp/output.mp4');
// ... wait some time ...
obs.stopRecording();
obs.shutdown();                 // clean up
```

`startRecording` will create a simple screen capture source, H.264/AAC
encoders and an `ffmpeg_muxer` output writing to the given file path.
Only very basic settings are currently used.
