const obs = require('..');
console.log('OBS Version:', obs.obsVersion());
console.log('Init OBS:', obs.init());
console.log('Start recording:', obs.startRecording('/tmp/test.mp4'));
setTimeout(() => {
  obs.stopRecording();
  obs.shutdown();
  console.log('Done');
}, 1000);
