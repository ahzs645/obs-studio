const obs = require('..');
console.log('OBS Version:', obs.obsVersion());
console.log('Has Screen Permission:', obs.checkScreenPermission());
console.log('Init OBS:', obs.init());
const displays = obs.listDisplays();
console.log('Found displays:', displays);
console.log('Start recording:', obs.startRecording('/tmp/test.mp4', {
  displayId: displays[0] ? displays[0].id : '',
  width: 640,
  height: 480,
  fps: 30
}));
setTimeout(() => {
  obs.stopRecording();
  obs.shutdown();
  console.log('Done');
}, 1000);
