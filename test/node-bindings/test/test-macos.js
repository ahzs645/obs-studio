const obs = require('..');
console.log('OBS Version:', obs.obsVersion());
console.log('Has Screen Permission:', obs.checkScreenPermission());
console.log('Init OBS:', obs.init());
obs.shutdown();
