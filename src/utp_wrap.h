#ifndef UTP_WRAP_H_
#define UTP_WRAP_H_

#include <node.h>
#include <node_buffer.h>
#include <v8.h>

#include "utp.h"
#include "utp_utils.h"
using namespace v8;


class UTPWrap: public node::ObjectWrap {
public:
  UTPWrap(char* address, int port);
  UTPWrap(UTPSocket* _us);
  size_t getReadBufferSize();
  void onGotIncomingConnection(UTPWrap* wrap);
  void onWrite(byte* bytes, size_t count);
  static void Initialize(Handle<Object> exports);
  static Handle<Value> New(const Arguments& args);
  static Handle<Value> IsIncomingUTP(const Arguments& args);
  static Handle<Value> GetPacketSize(const Arguments& args);
  static Handle<Value> Connect(const Arguments& args);
  static Handle<Value> Write(const Arguments& args);
  static Handle<Value> Close(const Arguments& args);
  static Handle<Value> CheckTimeouts(const Arguments& args);
  UTPSocket *utp_socket;
};

#endif // UTP_WRAP_H_