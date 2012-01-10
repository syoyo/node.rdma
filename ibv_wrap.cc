// 
// Copyright 2011 Light Transport Entertainment, Inc.
//
// Licensed under BSD license
//

// node.js and v8
#include <v8.h>
#include <node.h>
#include <node_buffer.h>

// C/C++
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

// IB Verbs
#include <infiniband/verbs.h>


using namespace v8;
using namespace node;

Persistent<Function> ibvConstructor;

static Persistent<String> family_symbol;
static Persistent<String> address_symbol;
static Persistent<String> port_symbol;

class IBV : public node::ObjectWrap {
public:

  static void Initialize(Handle<Object> target) {

    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    t->SetClassName(String::NewSymbol("IBV"));

    t->InstanceTemplate()->SetInternalFieldCount(1);



    // API
    NODE_SET_PROTOTYPE_METHOD(t, "pd", PD);
    NODE_SET_PROTOTYPE_METHOD(t, "comp_channel", CompChannel);
    NODE_SET_PROTOTYPE_METHOD(t, "cq", CQ);
    NODE_SET_PROTOTYPE_METHOD(t, "resize_cq", ResizeCQ);
    NODE_SET_PROTOTYPE_METHOD(t, "qp", QP);
    NODE_SET_PROTOTYPE_METHOD(t, "mr", MR);

    NODE_SET_PROTOTYPE_METHOD(t, "query_device", QueryDevice);
    NODE_SET_PROTOTYPE_METHOD(t, "query_port", QueryPort);

    NODE_SET_PROTOTYPE_METHOD(t, "post_send", PostSend);

    // @todo {}
    //NODE_SET_PROTOTYPE_METHOD(t, "make_send_wr", MakeSendWR);


    ibvConstructor = Persistent<Function>::New(t->GetFunction());

    target->Set(String::NewSymbol("IBV"), ibvConstructor);

  }

private:

  static Handle<Value> PD(const Arguments& args) {
    HandleScope scope;

    IBV *ibv = ObjectWrap::Unwrap<IBV>(args.This());

    ibv->pd_ = ibv_alloc_pd(ibv->ctx_);
    assert(ibv->pd_);

  }

  static Handle<Value> CompChannel(const Arguments& args) {
    HandleScope scope;

    // num_cq
    assert(args.Length() >= 1);
    assert(args[0]->IsInt32());

    int num_cq = args[0]->Int32Value();

    IBV *ibv = ObjectWrap::Unwrap<IBV>(args.This());

    ibv->comp_channel_ = ibv_create_comp_channel(ibv->ctx_);
    assert(ibv->comp_channel_);

  }

  static Handle<Value> CQ(const Arguments& args) {
    HandleScope scope;

    // num_cq
    assert(args.Length() >= 1);
    assert(args[0]->IsInt32());

    int num_cq = args[0]->Int32Value();

    IBV *ibv = ObjectWrap::Unwrap<IBV>(args.This());

    ibv->cq_ = ibv_create_cq(ibv->ctx_, num_cq, NULL, ibv->comp_channel_, 0);
    assert(ibv->cq_);

    int ret = ibv_req_notify_cq(ibv->cq_, 0);
    assert(ret == 0);

  }

  static Handle<Value> ResizeCQ(const Arguments& args) {
    HandleScope scope;

    // num_cq
    assert(args.Length() >= 1);
    assert(args[0]->IsInt32());

    int num_cq = args[0]->Int32Value();

    IBV *ibv = ObjectWrap::Unwrap<IBV>(args.This());

    int ret = ibv_resize_cq(ibv->cq_, num_cq);
    assert(ret == 0);

  }

  static Handle<Value> QP(const Arguments& args) {
    HandleScope scope;

    // (max_send_wr, max_recv_wr)
    assert(args.Length() >= 2);
    assert(args[0]->IsInt32());
    assert(args[1]->IsInt32());

    int num_cq = args[0]->Int32Value();

    IBV *ibv = ObjectWrap::Unwrap<IBV>(args.This());

    struct ibv_qp_init_attr attr;

    attr.send_cq = ibv->cq_;
    attr.recv_cq = ibv->cq_;
    attr.qp_type = IBV_QPT_RC;  // @fixme

    attr.cap.max_send_wr = args[0]->Uint32Value();
    attr.cap.max_recv_wr = args[1]->Uint32Value();
    attr.cap.max_send_sge = 1;  // @fixme
    attr.cap.max_recv_sge = 1;  // @fixme
    // @note
    // sqr, sq_sig_all, max_inline_data
    ibv->qp_ = ibv_create_qp(ibv->pd_, &attr);
    assert(ibv->qp_);

  }

  static Handle<Value> MR(const Arguments& args) {
    HandleScope scope;

    // (buffer, access_flag)
    assert(args.Length() >= 2);
    assert(args[0]->IsObject());
    assert(args[1]->IsInt32());

    Buffer *buffer = ObjectWrap::Unwrap<Buffer>(args[0]->ToObject());
    int access_flag = args[1]->Int32Value();

    IBV *ibv = ObjectWrap::Unwrap<IBV>(args.This());

    struct ibv_qp_init_attr attr;

    // @todo { Check buffer's address is persistent during IB operation. }
    ibv->mr_ = ibv_reg_mr(ibv->pd_, Buffer::Data(buffer), Buffer::Length(buffer), access_flag);
    ibv->qp_ = ibv_create_qp(ibv->pd_, &attr);
    assert(ibv->qp_);

  }

  static Handle<Value> PostSend(const Arguments& args) {
    HandleScope scope;

    // (buffer, access_flag)
    assert(args.Length() >= 2);
    assert(args[0]->IsObject());
    assert(args[1]->IsInt32());

    IBV *ibv = ObjectWrap::Unwrap<IBV>(args.This());

    struct ibv_send_wr *wr; // @fixme
    assert(0);  // @todo { implement }

    struct ibv_send_wr *bad_wr = NULL;
    int ret = ibv_post_send(ibv->qp_, wr, &bad_wr);
    assert(ret == 0);

  }

  static Handle<Value> QueryDevice(const Arguments& args) {
    HandleScope scope;

    IBV *ibv = ObjectWrap::Unwrap<IBV>(args.This());

    struct ibv_device_attr attr;
    int ret = ibv_query_device(ibv->ctx_, &attr);
    assert(ret == 0);

    Local<Object> devattr = Object::New();

    const char* atomic_cap_str[] = {
      "IBV_ATOMIC_NONE",
      "IBV_ATOMIC_HCA",
      "IBV_ATOMIC_GLOB"
    };

#define SET_INT_FIELD(str, val) { \
    devattr->Set(String::New((str)), Integer::New((val))); }

    devattr->Set(String::New("fw_ver"), String::New(attr.fw_ver));
    SET_INT_FIELD("node_guid", attr.node_guid);
    SET_INT_FIELD("sys_image_guid", attr.sys_image_guid);
    SET_INT_FIELD("max_mr_size", attr.max_mr_size);
    SET_INT_FIELD("page_size_cap", attr.page_size_cap);
    SET_INT_FIELD("vendor_id", attr.vendor_id);
    SET_INT_FIELD("vendor_part_id", attr.vendor_part_id);
    SET_INT_FIELD("hw_ver", attr.hw_ver);
    SET_INT_FIELD("max_qp", attr.max_qp);
    SET_INT_FIELD("max_qp_wr", attr.max_qp_wr);
    SET_INT_FIELD("device_cap_flags", attr.device_cap_flags);
    SET_INT_FIELD("max_sge", attr.max_sge);
    SET_INT_FIELD("max_sge_rd", attr.max_sge_rd);
    SET_INT_FIELD("max_cq", attr.max_cq);
    SET_INT_FIELD("max_cqe", attr.max_cqe);
    SET_INT_FIELD("max_qp_rd_atom", attr.max_qp_rd_atom);
    SET_INT_FIELD("max_ee_rd_atom", attr.max_ee_rd_atom);
    SET_INT_FIELD("max_res_rd_atom", attr.max_res_rd_atom);
    SET_INT_FIELD("max_qp_init_rd_atom", attr.max_qp_init_rd_atom);
    SET_INT_FIELD("max_ee_init_rd_atom", attr.max_ee_init_rd_atom);

    devattr->Set(String::New("atomic_cap"), String::New(atomic_cap_str[attr.atomic_cap]));
    
    SET_INT_FIELD("max_ee", attr.max_ee);
    SET_INT_FIELD("max_rdd", attr.max_rdd);
    SET_INT_FIELD("max_mw", attr.max_mw);
    SET_INT_FIELD("max_raw_ipv6_qp", attr.max_raw_ipv6_qp);
    SET_INT_FIELD("max_raw_ethy_qp", attr.max_raw_ethy_qp);
    SET_INT_FIELD("max_mcast_grp", attr.max_mcast_grp);
    SET_INT_FIELD("max_mcast_qp_attach", attr.max_mcast_qp_attach);
    SET_INT_FIELD("max_total_mcast_qp_attach", attr.max_total_mcast_qp_attach);
    SET_INT_FIELD("max_ah", attr.max_ah);
    SET_INT_FIELD("max_fmr", attr.max_fmr);
    SET_INT_FIELD("max_map_per_fmr", attr.max_map_per_fmr);
    SET_INT_FIELD("max_srq", attr.max_srq);
    SET_INT_FIELD("max_srq_wr", attr.max_srq_wr);
    SET_INT_FIELD("max_srq_sge", attr.max_srq_sge);
    SET_INT_FIELD("max_pkeys", attr.max_pkeys);
    SET_INT_FIELD("local_ca_ack_delay", attr.local_ca_ack_delay);
    SET_INT_FIELD("phys_port_cnt", attr.phys_port_cnt);

#undef SET_INT_FIELD

    return scope.Close(devattr);
  }

  static Handle<Value> QueryPort(const Arguments& args) {
    HandleScope scope;

    // port
    assert(args.Length() >= 1);
    assert(args[0]->IsInt32());

    int port = args[0]->Uint32Value();

    IBV *ibv = ObjectWrap::Unwrap<IBV>(args.This());

    struct ibv_port_attr attr;
    int ret = ibv_query_port(ibv->ctx_, (uint8_t)port, &attr);
    assert(ret == 0);

    Local<Object> portattr = Object::New();

    const char* mtu_str[] = {
      "IBV_MTU_256",
      "IBV_MTU_512",
      "IBV_MTU_1024",
      "IBV_MTU_2048",
      "IBV_MTU_4096"
    };

    portattr->Set(String::New("state"), String::New(ibv_port_state_str(attr.state)));
    portattr->Set(String::New("max_mtu"), String::New(mtu_str[attr.max_mtu]));
    portattr->Set(String::New("active_mtu"), String::New(mtu_str[attr.active_mtu]));
    portattr->Set(String::New("gid_tbl_len"), Integer::New(attr.gid_tbl_len));
    portattr->Set(String::New("port_cap_flags"), Integer::New(attr.port_cap_flags));
    portattr->Set(String::New("max_msg_sz"), Integer::New(attr.max_msg_sz));
    portattr->Set(String::New("bad_pkey_cntr"), Integer::New(attr.bad_pkey_cntr));
    portattr->Set(String::New("qkey_viol_cntr"), Integer::New(attr.qkey_viol_cntr));
    portattr->Set(String::New("pkey_tbl_len"), Integer::New(attr.pkey_tbl_len));
    portattr->Set(String::New("lid"), Integer::New(attr.lid));
    portattr->Set(String::New("sm_lid"), Integer::New(attr.sm_lid));
    portattr->Set(String::New("lmc"), Integer::New(attr.sm_lid));
    portattr->Set(String::New("max_vl_num"), Integer::New(attr.max_vl_num));
    portattr->Set(String::New("sm_sl"), Integer::New(attr.sm_sl));
    portattr->Set(String::New("subnet_timeout"), Integer::New(attr.subnet_timeout));
    portattr->Set(String::New("init_type_reply"), Integer::New(attr.init_type_reply));
    portattr->Set(String::New("active_width"), Integer::New(attr.active_width));
    portattr->Set(String::New("active_speed"), Integer::New(attr.active_speed));
    portattr->Set(String::New("phys_state"), Integer::New(attr.phys_state));

    return scope.Close(portattr);
  }


  static Handle<Value> CreateEventChannel(const Arguments& args) {
    HandleScope scope;

    IBV *ibv = ObjectWrap::Unwrap<IBV>(args.This());

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
      (new IBV())->Wrap(args.This());
    } catch (const char* msg) {
      return ThrowException(Exception::Error(String::New(msg)));
    }

    return args.This();
  }


  IBV() {
  }

  ~IBV() {
    int ret;

    if (pd_) {
      ret = ibv_dealloc_pd(pd_);
      assert(ret == 0);
    }

    if (cq_) {
      ret = ibv_destroy_cq(cq_);
      assert(ret == 0);
    }

    if (comp_channel_) {
      ret = ibv_destroy_comp_channel(comp_channel_);
      assert(ret == 0);
    }

    if (ctx_) {
      ret = ibv_close_device(ctx_);
      assert(ret == 0);
    }
  }


  // members
  struct ibv_context *ctx_;
  struct ibv_pd *pd_;
  struct ibv_cq *cq_;
  struct ibv_qp *qp_;
  struct ibv_comp_channel *comp_channel_;
  struct ibv_mr *mr_; // @fixme

};


// Notes
// - ibv_devinfo
// - ibv_context
// - ibv_pd
// - ibv_qp
// - ibv_cq
// - ibv_comp_channel
// - ibv_mr
// - ibv_qp_init_attr
// - ibv_wc
// - ibv_sge
// - ibv_send_wr
// - ibv_recv_wr

