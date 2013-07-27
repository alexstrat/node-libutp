### Build

```bash
$ node-gyp configure --debug && node-gyp build --debug
# or
$node-gyp rebuild --debug
```

### Run

In `libutp/utp_file`:

```bash
$ make
$ ./utp_recv log_rcv.log 8001 received.file
```

And then:

```bash
$ NODE_DEBUG=utp ./utp_send test.js.log localhost:8001 path/to/file
```
