#define BUILDING_NODE_EXTENSION

#include <node.h>
#include <v8.h>
#include "utp.h"
#include "utp_utils.h"
#include "utp_wrap.h"
#include <sstream>


using namespace v8;

Persistent<Object> utp_log_obj;

struct UTPProxyCallbackTable {
  Local<Function> send_to;
};

void utp_log(char const* fmt, ...)
{
  char log_line[500];

  va_list vl;
  va_start(vl, fmt);
  vsprintf(log_line, fmt, vl);

  HandleScope scope;
  Local<Value> argv[1] = {
    scope.Close(String::New(log_line))
  };

  node::MakeCallback(utp_log_obj, "on_data", 1, argv);

  va_end(vl);

}

void send_to_proxy(void *userdata, const byte *p, size_t len, const struct sockaddr *to, socklen_t tolen)
{
  const unsigned argc = 1;
  Local<Value> argv[argc] = { Local<Value>::New(String::New("hello world")) };
  ((UTPProxyCallbackTable*)userdata)->send_to->Call(Context::GetCurrent()->Global(), argc, argv);
}

Handle<Value>  Get_ms(const Arguments& args) {
  HandleScope scope;
  return scope.Close(Number::New(UTP_GetMilliseconds()));
}

Handle<Value> UTP_Create(const Arguments& args) {
  HandleScope scope;

  UTPProxyCallbackTable cbs;
  cbs.send_to = Local<Function>::Cast(args[0]);

  sockaddr_in sin;
  UTPSocket *utp_socket = UTP_Create(&send_to_proxy, &cbs, (const struct sockaddr*)&sin, sizeof(sin));

  return scope.Close(String::New("world"));
}

void init(Handle<Object> exports) {
  exports->Set(String::NewSymbol("UTP_Create"),
      FunctionTemplate::New(UTP_Create)->GetFunction());

  exports->Set(String::NewSymbol("UTP_GetMilliseconds"),
      FunctionTemplate::New(Get_ms)->GetFunction());

  utp_log_obj = Persistent<Object>::New(Object::New());
  exports->Set(String::New("utp_log"), utp_log_obj);
  UTPWrap::Initialize(exports);

}

NODE_MODULE(utp, init)
