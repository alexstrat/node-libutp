var stream = require('stream');
var util = require('util');
var dns = require('dns');
var assert = require('assert');

var addon = require('bindings')('utp.node');

var CHECK_TIMEOUTS_INTERVAL = 50; // ms
var checkTimeout = new CheckTimeoutsManager(addon.checkTimeouts, CHECK_TIMEOUTS_INTERVAL);

// handle implements event emitter
var EventEmitter = require('events').EventEmitter;
for (var meth in EventEmitter.prototype) {
  addon.UTP.prototype[meth] = EventEmitter.prototype[meth];
}

// classic srtream
exports.UTPLog = new (require('stream'))();
exports.UTPLog.readable = true;
addon.utp_log.on_data = function(str) {
  exports.UTPLog.emit('data', '['+addon.UTP_GetMilliseconds()+'] '+str+'\n');
};


var debug;
if (process.env.NODE_DEBUG && /utp/.test(process.env.NODE_DEBUG)) {
  var pid = process.pid;
  debug = function(x) {
    // if console is not set up yet, then skip this.
    if (!console.error)
      return;
    console.error('UTP: %d', pid,
                  util.format.apply(util, arguments).slice(0, 500));
  };
} else {
  debug = function() { };
}


// Socket:
//
//
//

function Socket(udp_socket) {
  stream.Duplex.call(this);
  this._udp_socket = udp_socket;
  this._handle = null;

  this.on('finish', onSocketFinish);
}

util.inherits(Socket, stream.Duplex);

function onSocketFinish() {
  debug('onSocketFinish');
  if(this._connecting) {
    return this.once('connect', onSocketFinish);
  }
  this.destroy();
}

Socket.prototype.connect = function(address, port) {

  this._connecting = true;
  var that = this;

  dns.lookup(address, 4, function(error, ip) {

    that._handle = new addon.UTP(ip, port);

    that._handle.on('udpsend', function(buffer, rinfo) {
      that._udp_socket.send(buffer, 0, buffer.length, rinfo.port, rinfo.address);
    });

    that._handle.got_incoming_connection = function() {
      console.log(arguments);
    };

    that._handle.get_rb_size = function() {
      // TODO
      return 0;
    };

    that._udp_socket.on('message', function(buffer, rinfo) {
      that._handle.isIncomingUTP(buffer, rinfo.address, rinfo.port);
    });

    // proxy connect
    that._handle.on('connect',function() {
      that._connecting = false;
      that.emit('connect');
    });

    that._handle.on('eof', function() {
      debug('handle emits eof');
      that.emit('end');
    });
    // set timer
    checkTimeout.register(that);
    that._handle.on('destroying', function() {
      debug('handle emits destroying');
      checkTimeout.deregister(that);
      that.emit('close');
    });

    that._handle.connect();
  });
};

Socket.prototype._write = function(chunk, encoding, next) {
  if(this._connecting) {
    var _args = arguments;
    this.once('connect', function() {
      this._write(chunk, encoding, next);
    });
    return;
  }

  var req = new WriteReq(this._handle, chunk);
  req.cb = next;
  req.tryWriting();
};

Socket.prototype.destroy = function() {
  debug('destroy');
  this._handle.close();
};

exports.Socket = Socket;



// CheckTimeoutsManager:
//
// Manage checkTimouts interva of libutp:
//  - start it if needed 
//  - stop if not needed anymore
//

function CheckTimeoutsManager(checkTimeouts, interval) {
  this.checkTimeouts = checkTimeouts;
  this.interval = interval;
  this.started = false;
  this.registered = 0;
}

CheckTimeoutsManager.prototype._start = function() {
  if(this.started) return;
  this.intervalID = setInterval(this.checkTimeouts, this.interval);
  this.started = true;
};

CheckTimeoutsManager.prototype._stop = function() {
  clearInterval(this.intervalID);
  this.started = false;
};

CheckTimeoutsManager.prototype.register = function(s) {
  this.registered ++;

  if(!this.started && this.registered > 0)
    this._start();
};

CheckTimeoutsManager.prototype.deregister = function(s) {
  this.registered --;

  if(this.started && this.registered === 0)
    this._stop();
};



// WriteReq:
//
// Embodies a write request on the underlying utp socket
// send the buffer chunk by chunk when socket is ready (writable)
//

function WriteReq(handle, buffer) {
  this.handle = handle;
  this.buffer = buffer;
  this.cursor = 0;
  var that = this;

  this._onwritable = function() {
    that.tryWriting();
  };
  this.handle.on('writable', this._onwritable);
}

WriteReq.prototype.tryWriting = function() {
  var that = this;

  this.handle.on_write = function(count) {
    var chunk = that.buffer.slice(that.cursor, that.cursor+count);
    that.cursor += count;
    return chunk;
  };

  var ret = this.handle.write(this.buffer.length - this.cursor);

  if(this.cursor >= this.buffer.length)
    this.finish();

  return ret;
};

WriteReq.prototype.finish = function() {
  this.handle.removeListener('writable', this._onwritable);
  if (this.cb) this.cb();
};



