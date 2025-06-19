const obs = require('..');
console.log('OBS Version:', obs.obsVersion());
console.log('Init OBS:', obs.init());
obs.shutdown();
