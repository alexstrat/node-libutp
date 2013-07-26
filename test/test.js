var utp = require('../lib/utp');
var udp = require('dgram').createSocket('udp4');

var s = new utp.Socket(udp);
s.on('close', function() {
  udp.close();
});

var file = process.argv[2];
if(!file) throw new Error('I need a file !');

utp.UTPLog.pipe(require('fs').createWriteStream(__dirname+'/test.js.log'));
// var s = new require('net').Socket();

var input = require('fs').createReadStream(file);
s.connect('localhost', 8001);
input.pipe(s);
