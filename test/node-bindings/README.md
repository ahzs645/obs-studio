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
// choose display or window via options
const displays = obs.listDisplays();
const windows = obs.listWindows();
obs.startRecording('/tmp/output.mp4', {
  displayId: displays[0].id, // or windowId: windows[0].id
  width: 1920,
  height: 1080,
  fps: 60
});
// ... wait some time ...
obs.stopRecording();
obs.shutdown();                 // clean up
```

`listDisplays()` and `listWindows()` enumerate available screen sources on the
current platform. `startRecording(path, options)` now accepts a second argument
to specify recording width/height, fps and which display or window to capture.
Settings still use simple defaults for encoding.
