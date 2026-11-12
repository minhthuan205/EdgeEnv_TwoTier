// Optional test broker, loopback only. Install: npm install --prefix .test-tools aedes@0.51.3
const aedes = require('../.test-tools/node_modules/aedes')();
const server = require('node:net').createServer(aedes.handle);
server.listen(1883, '127.0.0.1', () => console.log('MQTT test broker: 127.0.0.1:1883 (no authentication, local only)'));
process.on('SIGINT', () => server.close(() => aedes.close(() => process.exit(0))));
