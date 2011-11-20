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

    // @todo {}
    //NODE_SET_PROTOTYPE_METHOD(t, "create_id", CreateID);
    //NODE_SET_PROTOTYPE_METHOD(t, "destroy_id", DestroyID);
    //NODE_SET_PROTOTYPE_METHOD(t, "bind_addr", BindAddr);
    //NODE_SET_PROTOTYPE_METHOD(t, "resolve_addr", ResolveAddr);
    //NODE_SET_PROTOTYPE_METHOD(t, "resolve_route", ResolveRoute);
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
    //NODE_SET_PROTOTYPE_METHOD(t, "get_cm_event", GetCMEvent);
    //NODE_SET_PROTOTYPE_METHOD(t, "ack_cm_event", AckCMEvent);
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

    Local<Object> channel = Object::New();

    struct rdma_create_id *id;

    channel->SetPointerInInternalField(0, id);

    return scope.Close(channel);
    
  }

  static Handle<Value> DestroyEventChannel(const Arguments& args) {

    return args.This();

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


  RDMA_CM() {
  }

  ~RDMA_CM() { }


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
