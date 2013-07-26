#define BUILDING_NODE_EXTENSION
#include <node.h>
#include <v8.h>

#include "utp.h"
#include "utp_wrap.h"
#include "utp_utils.h"
using namespace v8;

Local<Object> AddressToJS(const sockaddr* addr);
// Handle<Object> _exports;

// proc
void incoming_proc(void *userdata, struct UTPSocket* s) {
  UTPWrap* wrap = new UTPWrap(s);
  ((UTPWrap*)userdata)->onGotIncomingConnection(wrap);
}
void send_to_proc(void *userdata, const byte *p, size_t len, const struct sockaddr *to, socklen_t tolen) {
  UTPWrap* wrap = (UTPWrap*)userdata;
  HandleScope scope;

  // transform into buffer
  node::Buffer* bf = node::Buffer::New(len);
  memcpy(node::Buffer::Data(bf), p, len);

  // construct arguments
  Local<Value> argv[3] = {
    Local<String>::New(String::New("udpsend")),
    Local<Object>::New(bf->handle_),
    AddressToJS(to)
  };
  
  node::MakeCallback(wrap->handle_, "emit", 3, argv);

}

// define utp callbacks
void on_read(void *userdata, const byte *bytes, size_t count) {
  UTPWrap* wrap = (UTPWrap*)userdata;
  HandleScope scope;

  node::Buffer* bf = node::Buffer::New(count);
  memcpy(node::Buffer::Data(bf), bytes, count);

  Local<Value> argv[2] = {
    Local<String>::New(String::New("message")),
    Local<Object>::New(bf->handle_)
  };

  node::MakeCallback(wrap->handle_, "emit", 2, argv);

}
void on_write(void *userdata, byte* bytes, size_t count) {
  ((UTPWrap*)userdata)->onWrite(bytes, count);
}
size_t get_rb_size(void *userdata) {
  return ((UTPWrap*)userdata)->getReadBufferSize();
}
void on_state_change(void *userdata, int state) {
  UTPWrap* wrap = (UTPWrap*) userdata;

  HandleScope scope;

  const char* event;
  switch(state) {
    case UTP_STATE_CONNECT:
      event = "connect";
      break;

    case UTP_STATE_WRITABLE:
      event = "writable";
      break;

    case UTP_STATE_EOF:
      event = "eof";
      break;

    case UTP_STATE_DESTROYING:
      event = "destroying";
      break;
  }

  Local<Value> argv[1] = {
    Local<String>::New(String::New(event))
  };

  node::MakeCallback(wrap->handle_, "emit", 1, argv);
}

void on_error(void *userdata, int errcode) {
  UTPWrap* wrap = (UTPWrap*) userdata;

  HandleScope scope;

  Local<Value> argv[2] = {
    String::New("error"),
    scope.Close(Number::New(errcode))
  };

  node::MakeCallback(wrap->handle_, "emit", 2, argv);
}

void on_overhead(void *userdata, bool send, size_t count, int type) {
  // TODO
}

UTPFunctionTable utp_callbacks = {
  &on_read,
  &on_write,
  &get_rb_size,
  &on_state_change,
  &on_error,
  &on_overhead
};

UTPWrap::UTPWrap(char* address, int port):node::ObjectWrap() {

  struct sockaddr_in sin = uv_ip4_addr(address, port);
  struct sockaddr* addr = (struct sockaddr*) &sin;

  utp_socket = UTP_Create(&send_to_proc, this, addr, sizeof(sockaddr));
  UTP_SetSockopt(utp_socket, SO_SNDBUF, 100*300);

  UTP_SetCallbacks(utp_socket, &utp_callbacks, this);

}

UTPWrap::UTPWrap(UTPSocket* _us):node::ObjectWrap() {
  utp_socket = _us;
  UTP_SetCallbacks(utp_socket, &utp_callbacks, this);
}

void UTPWrap::Initialize(Handle<Object> exports) {
  //_exports = exports;
  // Prepare constructor template
  Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
  tpl->SetClassName(String::NewSymbol("UTP"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  NODE_SET_PROTOTYPE_METHOD(tpl, "connect", Connect);
  NODE_SET_PROTOTYPE_METHOD(tpl, "isIncomingUTP", IsIncomingUTP);
  NODE_SET_PROTOTYPE_METHOD(tpl, "write", Write);
  NODE_SET_PROTOTYPE_METHOD(tpl, "getPacketSize", GetPacketSize);
  NODE_SET_PROTOTYPE_METHOD(tpl, "close", Close);

  Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
  exports->Set(String::NewSymbol("UTP"), constructor);
  exports->Set(String::NewSymbol("checkTimeouts"), FunctionTemplate::New(CheckTimeouts)->GetFunction());
}

Handle<Value> UTPWrap::New(const Arguments& args) {

  // called with address, port
  String::Utf8Value address(args[0]);
  int port = args[1]->Uint32Value();

   UTPWrap* obj = new UTPWrap(*address, port);
   obj->Wrap(args.This());

   return args.This();
}

void UTPWrap::onWrite(byte* bytes, size_t count) {
  HandleScope scope;
  Local<Value> argv[1] = {
    scope.Close(Number::New(count))
  }; 
  Handle<Value> buf = node::MakeCallback(this->handle_, "on_write", 1, argv);
  byte* data = (byte *)node::Buffer::Data(buf);

  memcpy(bytes, data, count);
}

size_t UTPWrap::getReadBufferSize() {
  Local<Value> argv[0] = {}; 
  Handle<Value> size = node::MakeCallback(this->handle_, "get_rb_size", 0, argv);

  return (size_t) size->Uint32Value();
}

void UTPWrap::onGotIncomingConnection(UTPWrap* wrap) {
  HandleScope scope;

  Local<Value> argv[2] = {
    scope.Close(wrap->handle_)
  };
  node::MakeCallback(this->handle_, "got_incoming_connection", 1, argv);
}

Handle<Value> UTPWrap::IsIncomingUTP(const Arguments& args) {
  HandleScope scope;
  // called with buffer, address, port
  Local<Object> buf = args[0]->ToObject();
  String::Utf8Value address(args[1]);
  const int port = args[2]->Uint32Value();

  struct sockaddr_in sin = uv_ip4_addr(*address, port);
  struct sockaddr* addr = (struct sockaddr*) &sin;


  const byte* data = (byte *)node::Buffer::Data(buf);
  size_t len = node::Buffer::Length(buf);

  UTPWrap* obj = node::ObjectWrap::Unwrap<UTPWrap>(args.This());

  bool incoming = UTP_IsIncomingUTP(&incoming_proc,
               &send_to_proc, obj,
               data, len, addr, sizeof(*addr));

  return scope.Close(Boolean::New(incoming));
}

Handle<Value> UTPWrap::Close(const Arguments& args) {
  HandleScope scope;
  UTPWrap* obj = node::ObjectWrap::Unwrap<UTPWrap>(args.This());
  UTP_Close(obj->utp_socket);
  return scope.Close(Undefined());
}

Handle<Value> UTPWrap::GetPacketSize(const Arguments& args) {
  HandleScope scope;
  UTPWrap* obj = node::ObjectWrap::Unwrap<UTPWrap>(args.This());
  size_t size = UTP_GetPacketSize(obj->utp_socket);
  return scope.Close(Number::New(size));
}

Handle<Value> UTPWrap::Write(const Arguments& args) {
  HandleScope scope;
  UTPWrap* obj = node::ObjectWrap::Unwrap<UTPWrap>(args.This());

  size_t count = (size_t) args[0]->Uint32Value();
  //bool more = true;
  bool more = UTP_Write(obj->utp_socket, count);

  return scope.Close(Boolean::New(more));
}

Handle<Value> UTPWrap::Connect(const Arguments& args) {
  HandleScope scope;
  UTPWrap* obj = node::ObjectWrap::Unwrap<UTPWrap>(args.This());
  UTP_Connect(obj->utp_socket);
  return scope.Close(Undefined());
}

Handle<Value> UTPWrap::CheckTimeouts(const Arguments& args) {
  HandleScope scope;
  UTP_CheckTimeouts();
  return scope.Close(Undefined());
}

// grabbed from tcp_wrap.cc
Local<Object> AddressToJS(const sockaddr* addr) {
  static Persistent<String> address_sym;
  static Persistent<String> family_sym;
  static Persistent<String> port_sym;
  static Persistent<String> ipv4_sym;
  static Persistent<String> ipv6_sym;

  HandleScope scope;
  char ip[INET6_ADDRSTRLEN];
  const sockaddr_in *a4;
  const sockaddr_in6 *a6;
  int port;

  if (address_sym.IsEmpty()) {
    address_sym = NODE_PSYMBOL("address");
    family_sym = NODE_PSYMBOL("family");
    port_sym = NODE_PSYMBOL("port");
    ipv4_sym = NODE_PSYMBOL("IPv4");
    ipv6_sym = NODE_PSYMBOL("IPv6");
  }

  Local<Object> info = Object::New();

  switch (addr->sa_family) {
  case AF_INET6:
    a6 = reinterpret_cast<const sockaddr_in6*>(addr);
    uv_inet_ntop(AF_INET6, &a6->sin6_addr, ip, sizeof ip);
    port = ntohs(a6->sin6_port);
    info->Set(address_sym, String::New(ip));
    info->Set(family_sym, ipv6_sym);
    info->Set(port_sym, Integer::New(port));
    break;

  case AF_INET:
    a4 = reinterpret_cast<const sockaddr_in*>(addr);
    uv_inet_ntop(AF_INET, &a4->sin_addr, ip, sizeof ip);
    port = ntohs(a4->sin_port);
    info->Set(address_sym, String::New(ip));
    info->Set(family_sym, ipv4_sym);
    info->Set(port_sym, Integer::New(port));
    break;

  default:
    info->Set(address_sym, String::Empty());
  }

  return scope.Close(info);
}
