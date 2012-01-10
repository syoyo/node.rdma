// 
// Copyright 2011 Light Transport Entertainment, Inc.
//
// Licensed under BSD license
//

// node.js and v8
#include <v8.h>
#include <node.h>

// C/C++
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

// RDMA CM
#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>

using namespace v8;


Persistent<Function> rdma_cmConstructor;

static Persistent<String> family_symbol;
static Persistent<String> address_symbol;
static Persistent<String> port_symbol;

class RDMA_CM : public node::ObjectWrap {
public:

  static void Initialize(Handle<Object> target) {

    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    t->SetClassName(String::NewSymbol("RDMA_CM"));

    t->InstanceTemplate()->SetInternalFieldCount(1);



    // API
    NODE_SET_PROTOTYPE_METHOD(t, "create_event_channel", CreateEventChannel);
    NODE_SET_PROTOTYPE_METHOD(t, "destroy_event_channel", DestroyEventChannel);
    NODE_SET_PROTOTYPE_METHOD(t, "create_id", CreateID);
    NODE_SET_PROTOTYPE_METHOD(t, "resolve_addr", ResolveAddr);
    NODE_SET_PROTOTYPE_METHOD(t, "resolve_route", ResolveRoute);
    NODE_SET_PROTOTYPE_METHOD(t, "get_cm_event", GetCMEvent);
    NODE_SET_PROTOTYPE_METHOD(t, "ack_cm_event", AckCMEvent);

    // @todo {}
    //NODE_SET_PROTOTYPE_METHOD(t, "destroy_id", DestroyID);
    //NODE_SET_PROTOTYPE_METHOD(t, "bind_addr", BindAddr);
    //NODE_SET_PROTOTYPE_METHOD(t, "create_qp", CreateQP);
    //NODE_SET_PROTOTYPE_METHOD(t, "destroy_qp", DestroyQP);
    //NODE_SET_PROTOTYPE_METHOD(t, "listen", Listen);
    //NODE_SET_PROTOTYPE_METHOD(t, "connect", Connect);
    //NODE_SET_PROTOTYPE_METHOD(t, "accept", Accept);
    //NODE_SET_PROTOTYPE_METHOD(t, "reject", Reject);
    //NODE_SET_PROTOTYPE_METHOD(t, "notify", Notify);
    //NODE_SET_PROTOTYPE_METHOD(t, "disconnect", Disconnect);
    //NODE_SET_PROTOTYPE_METHOD(t, "join_multicast", JoinMulticast);
    //NODE_SET_PROTOTYPE_METHOD(t, "leave_multicast", LeaveMulticast);
    //NODE_SET_PROTOTYPE_METHOD(t, "get_src_port", GetSrcPort);
    //NODE_SET_PROTOTYPE_METHOD(t, "get_dst_port", GetDstPort);
    //NODE_SET_PROTOTYPE_METHOD(t, "get_local_addr", GetLocalAddr);
    //NODE_SET_PROTOTYPE_METHOD(t, "get_peer_addr", GetPeerAddr);
    //NODE_SET_PROTOTYPE_METHOD(t, "get_devices"  , GetDevices);
    //NODE_SET_PROTOTYPE_METHOD(t, "free_devices" , FreeDevices);
    //NODE_SET_PROTOTYPE_METHOD(t, "event_str"    , EventStr);
    //NODE_SET_PROTOTYPE_METHOD(t, "set_option"   , SetOption);
    //NODE_SET_PROTOTYPE_METHOD(t, "migrate_id"   , MigrateID);


    rdma_cmConstructor = Persistent<Function>::New(t->GetFunction());

    target->Set(String::NewSymbol("RDMA_CM"), rdma_cmConstructor);

  }

private:

  static Handle<Value> CreateEventChannel(const Arguments& args) {
    HandleScope scope;

    RDMA_CM *rdma_cm = ObjectWrap::Unwrap<RDMA_CM>(args.This());

    rdma_cm->event_channel_ = rdma_create_event_channel();
    assert(rdma_cm->event_channel_);

  }

  static Handle<Value> DestroyEventChannel(const Arguments& args) {

    RDMA_CM *rdma_cm = ObjectWrap::Unwrap<RDMA_CM>(args.This());

    rdma_destroy_id(rdma_cm->id_);

  }

  static Handle<Value> CreateID(const Arguments& args) {
    HandleScope scope;

    RDMA_CM *rdma_cm = ObjectWrap::Unwrap<RDMA_CM>(args.This());

    int ret = rdma_create_id(rdma_cm->event_channel_, &rdma_cm->id_, NULL, RDMA_PS_TCP);
    assert(ret == 0);

  }

  static Handle<Value> ResolveAddr(const Arguments& args) {
    HandleScope scope;

    RDMA_CM *rdma_cm = ObjectWrap::Unwrap<RDMA_CM>(args.This());

    // ("addr", "port")
    assert(args.Length() >= 2);
    assert(args[0]->IsString());
    assert(args[1]->IsString());

    String::AsciiValue ip_address(args[0]->ToString());
    String::AsciiValue port_str(args[1]->ToString());

    struct addrinfo *addr;

    int ret = getaddrinfo(*ip_address, *port_str, NULL, &addr);
    assert(ret == 0);

    int timeout_ms = 500; // 0.5 sec. @fixme
    ret = rdma_resolve_addr(rdma_cm->id_, NULL, addr->ai_addr, timeout_ms);
    assert(ret == 0);

    freeaddrinfo(addr);

  }

  static Handle<Value> GetCMEvent(const Arguments& args) {
    HandleScope scope;

    RDMA_CM *rdma_cm = ObjectWrap::Unwrap<RDMA_CM>(args.This());

    int ret = rdma_get_cm_event(rdma_cm->event_channel_, &rdma_cm->event_);
    assert(ret == 0);

    return scope.Close(Integer::New(ret));
  }

  static Handle<Value> AckCMEvent(const Arguments& args) {
    HandleScope scope;

    RDMA_CM *rdma_cm = ObjectWrap::Unwrap<RDMA_CM>(args.This());

    // ("addr", "port")
    assert(args.Length() >= 2);
    assert(args[0]->IsString());
    assert(args[1]->IsString());

    String::AsciiValue ip_address(args[0]->ToString());
    String::AsciiValue port_str(args[1]->ToString());

    struct addrinfo *addr;

    int ret = getaddrinfo(*ip_address, *port_str, NULL, &addr);
    assert(ret == 0);

    int timeout_ms = 500; // 0.5 sec. @fixme
    ret = rdma_resolve_addr(rdma_cm->id_, NULL, addr->ai_addr, timeout_ms);
    assert(ret == 0);

    freeaddrinfo(addr);

  }

  static Handle<Value> ResolveRoute(const Arguments& args) {
    HandleScope scope;

    RDMA_CM *rdma_cm = ObjectWrap::Unwrap<RDMA_CM>(args.This());

    int timeout_ms = 500; // 0.5 sec. @fixme
    int ret = rdma_resolve_route(rdma_cm->id_, timeout_ms);
    assert(ret == 0);


  }

  static Handle<Value>  New(const Arguments& args) {

    HandleScope scope;
    if (!args.IsConstructCall()) {
      return args.Callee()->NewInstance();
    }

    try {
      (new RDMA_CM())->Wrap(args.This());
    } catch (const char* msg) {
      return ThrowException(Exception::Error(String::New(msg)));
    }

    return args.This();
  }


  RDMA_CM() : id_(0), event_channel_(0), event_(0) {
  }

  ~RDMA_CM() {
    assert(id_ == NULL);
    assert(event_channel_ == NULL);
    assert(event_ == NULL);
  }


  struct rdma_cm_id         *id_;
  struct rdma_event_channel *event_channel_;
  struct rdma_cm_event      *event_;

};

//
// note:
//
// types:
//  - event_channel
//  - cm_id
//  - cm_event
//  - cm_event_type
//  - conn_param
//  - ibv_context
//  - ibv_pd
//  - ibv_event_type

extern "C" {
NODE_MODULE(rdma_cm, RDMA_CM::Initialize);
}
