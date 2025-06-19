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
// TODO: create sources/outputs and start recording
obs.shutdown();                 // clean up
```

The actual screen recording pipeline is not implemented here. These bindings
are meant to serve as groundwork for a full npm package built on top of
`libobs`.
