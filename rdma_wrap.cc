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

static const int RDMA_BUFFER_SIZE   = 1024;

typedef struct
{
    enum {
        MSG_MR,
        MSG_DONE
    } type;

    union {
        struct ibv_mr mr;
    } data;
} RDMAMessage;

typedef struct
{
    struct ibv_context*         ctx;            ///< Context
    struct ibv_pd*              pd;             ///< Protection Domain
    struct ibv_cq*              cq;             ///< Completion Queue
    struct ibv_comp_channel*    comp_channel;   ///< Completion Event Channel
} RDMAContext;


typedef struct
{
    bool                        connected;

    struct rdma_cm_id*          id;
    struct ibv_qp*              qp;

    struct ibv_mr*              recv_mr;
    struct ibv_mr*              send_mr;
    struct ibv_mr*              rdma_local_mr;
    struct ibv_mr*              rdma_remote_mr;

    struct ibv_mr               peer_mr;

    RDMAMessage*                recv_msg;
    RDMAMessage*                send_msg;

    char*                       rdma_local_region;
    char*                       rdma_remote_region;

    typedef enum {
        SS_INIT,
        SS_MR_SENT,
        SS_RDMA_SENT,
        SS_DONE_SENT
    } send_state_t;

    static send_state_t NextSendState(send_state_t state) {
        switch (state) {
        case SS_INIT:
            return SS_MR_SENT;
            break;
        case SS_MR_SENT:
            return SS_RDMA_SENT;
            break;
        case SS_RDMA_SENT:
            return SS_RDMA_SENT;
            break;
        case SS_DONE_SENT:
        default:
            assert(0);
            break;
        }
    }

    send_state_t                send_state;

    typedef enum {
        RS_INIT,
        RS_MR_RECV,
        RS_DONE_RECV
    } recv_state_t;

    static recv_state_t NextRecvState(recv_state_t state) {
        switch (state) {
        case RS_INIT:
            return RS_MR_RECV;
            break;
        case RS_MR_RECV:
            return RS_DONE_RECV;
            break;
        case RS_DONE_RECV:
        default:
            assert(0);
            break;
        }
    }


    recv_state_t                recv_state;

} RDMAConnection;

//static void RDMAConnection::send_state

//
// Builds internal RDMA context from ibv_context
//
static void BuildRDMAContext(struct ibv_context* verbs)
{
    RDMAContext* ctx = (RDMAContext*)malloc(sizeof(RDMAContext));
    ctx->ctx    = verbs;

    ctx->pd     = ibv_alloc_pd(ctx->ctx);
    if (!ctx->pd) {
        fprintf(stderr, "Failed to operate ibv_alloc_pd()\n");
        exit(-1);
    }

    ctx->comp_channel = ibv_create_comp_channel(ctx->ctx);
    if (!ctx->comp_channel) {
        fprintf(stderr, "Failed to operate ibv_create_comp_channel()\n");
        exit(-1);
    }

    int cqe = 10;   // # of completion queue elements. This could be arbitrary
    int comp_vector = 0;
    ctx->cq = ibv_create_cq(ctx->ctx, cqe, NULL, ctx->comp_channel, comp_vector);
    if (!ctx->cq) {
        fprintf(stderr, "Failed to operate ibv_create_qp()\n");
        exit(-1);
    }

    if (ibv_req_notify_cq(ctx->cq, 0)) {
        fprintf(stderr, "Failed to operate ibv_req_notify_cq()\n");
        exit(-1);
    }

    if (ibv_req_notify_cq(ctx->cq, 0)) {
        fprintf(stderr, "Failed to operate ibv_req_notify_cq()\n");
        exit(-1);
    }
}

//
// Buld queue pair attributes with RDMA context.
//
static void BuildQPAttr(const RDMAContext* ctx, struct ibv_qp_init_attr* qp_attr)
{
    memset(qp_attr, 0, sizeof(*qp_attr));

    qp_attr->send_cq = ctx->cq;
    qp_attr->recv_cq = ctx->cq;
    qp_attr->qp_type = IBV_QPT_RC;

    qp_attr->cap.max_send_wr = 10;
    qp_attr->cap.max_recv_wr = 10;
    qp_attr->cap.max_send_sge = 1;
    qp_attr->cap.max_recv_sge = 1;
    
}

static void RDMARegisterMemory(const RDMAContext* ctx, RDMAConnection* conn)
{
    conn->send_msg = (RDMAMessage*)malloc(sizeof(RDMAMessage));
    conn->recv_msg = (RDMAMessage*)malloc(sizeof(RDMAMessage));

    conn->rdma_local_region     = (char*)malloc(RDMA_BUFFER_SIZE);
    conn->rdma_remote_region    = (char*)malloc(RDMA_BUFFER_SIZE);

    conn->send_mr = ibv_reg_mr(ctx->pd, conn->send_msg, sizeof(RDMAMessage), IBV_ACCESS_LOCAL_WRITE);
    assert(conn->send_mr);

    conn->recv_mr = ibv_reg_mr(ctx->pd, conn->recv_msg, sizeof(RDMAMessage), IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    assert(conn->recv_mr);

    conn->rdma_local_mr = ibv_reg_mr(ctx->pd, conn->rdma_local_region, RDMA_BUFFER_SIZE, IBV_ACCESS_LOCAL_WRITE);
    assert(conn->rdma_local_mr);

    conn->rdma_remote_mr = ibv_reg_mr(ctx->pd, conn->rdma_remote_region, RDMA_BUFFER_SIZE, IBV_ACCESS_REMOTE_WRITE);
    assert(conn->rdma_remote_mr);
}

static void RDMAPostReceives(RDMAConnection* conn)
{
    struct ibv_recv_wr wr;
    struct ibv_recv_wr *bad_wr = NULL;
    struct ibv_sge sge;

    wr.wr_id = (uintptr_t)conn;
    wr.next = NULL;
    wr.sg_list = &sge;
    wr.num_sge = 1;

    sge.addr = (uintptr_t)conn->recv_msg;
    sge.length = sizeof(RDMAMessage);
    sge.lkey = conn->recv_mr->lkey;

    int ret = ibv_post_recv(conn->qp, &wr, &bad_wr);
    assert(!ret);
}

static void Connection(RDMAContext* ctx, struct rdma_cm_id* id)
{
    RDMAConnection* conn;
    struct ibv_qp_init_attr qp_attr;

    BuildRDMAContext(id->verbs);
    BuildQPAttr(ctx, &qp_attr);

    int ret = rdma_create_qp(id, ctx->pd, &qp_attr);
    assert(!ret);

    conn->id = id;
    conn->qp - id->qp;
    
    conn->send_state = RDMAConnection::SS_INIT;
    conn->recv_state = RDMAConnection::RS_INIT;

    conn->connected  = 0;

    RDMARegisterMemory(ctx, conn);
    RDMAPostReceives(conn);
    
}

//
// Destroy RDMA peer connection.
//
static void RDMADestroyConnection(void *context)
{
    RDMAConnection* conn = (RDMAConnection*)context;

    rdma_destroy_qp(conn->id);

    ibv_dereg_mr(conn->send_mr);
    ibv_dereg_mr(conn->recv_mr);
    ibv_dereg_mr(conn->rdma_local_mr);
    ibv_dereg_mr(conn->rdma_remote_mr);

    free(conn->send_msg);
    free(conn->recv_msg);
    free(conn->rdma_local_region);
    free(conn->rdma_remote_region);

    rdma_destroy_id(conn->id);

    free(conn);

}

static void RDMASendMessage(RDMAConnection* conn)
{
    struct ibv_send_wr wr;
    struct ibv_send_wr *bad_wr = NULL;
    struct ibv_sge sge;

    memset(&wr, 0, sizeof(wr));

    wr.wr_id = (uintptr_t)conn;
    wr.opcode = IBV_WR_SEND;
    wr.sg_list = &sge;
    wr.num_sge = 1;
    wr.send_flags = IBV_SEND_SIGNALED;

    sge.addr = (uintptr_t)conn->send_msg;
    sge.length = sizeof(RDMAMessage);
    sge.lkey = conn->send_mr->lkey;

    // @fixme { Is this required? }
    while (!conn->connected);

    int ret = ibv_post_send(conn->qp, &wr, &bad_wr);
    assert(!ret);

}

static void RDMASendMR(void *context)
{
    RDMAConnection* conn = (RDMAConnection*)context;

    conn->send_msg->type = RDMAMessage::MSG_MR;
    memcpy(&conn->send_msg->data.mr, conn->rdma_remote_mr, sizeof(struct ibv_mr));

    RDMASendMessage(conn);
}


static void OnConnect(void* context)
{
    ((RDMAConnection*)context)->connected = 1;
}

static void OnCompletion(struct ibv_wc* wc)
{
    RDMAConnection* conn = (RDMAConnection*)(uintptr_t)wc->wr_id;

    if (wc->status != IBV_WC_SUCCESS) {
        // die(expects IBV_WC_SUCCESS)
        exit(-1);
    }

    if (wc->opcode & IBV_WC_RECV) {
        conn->recv_state = RDMAConnection::NextRecvState(conn->recv_state);

        if (conn->recv_msg->type == RDMAMessage::MSG_MR) {
            memcpy(&conn->peer_mr, &conn->recv_msg->data.mr, sizeof(conn->peer_mr));
            RDMAPostReceives(conn);

            if (conn->send_state == RDMAConnection::SS_INIT) {
                RDMASendMR(conn);
            }
        }
    } else {
        conn->send_state = RDMAConnection::NextSendState(conn->send_state);
    }
}

static void* PollCQ(RDMAContext* s_ctx, void* ctx)
{
    struct ibv_cq* cq;
    struct ibv_wc  wc;

    while (1) {
        int ret = ibv_get_cq_event(s_ctx->comp_channel, &cq, &ctx);
        assert(!ret);
        ibv_ack_cq_events(cq, 1);
        ret = ibv_req_notify_cq(cq, 0);
        assert(!ret);

        while (ibv_poll_cq(cq, 1, &wc)) {
            OnCompletion(&wc);
        }
    }

    return NULL;
}

class
RDMAClientContext
{
public:

  void init(const char* ipaddr, int port) {

    struct addrinfo* addr;

    char buf[1024];
    sprintf(buf, "%d", port);

    int ret;
    ret = getaddrinfo(ipaddr, buf, NULL, &addr);
    assert(ret == 0);

    ret = rdma_create_id(ec, &conn, NULL, RDMA_PS_TCP);
    assert(!ret);

    int TIMEOUT_IN_MS = 500; // Arbitraray
    ret = rdma_resolve_addr(conn, NULL, addr->ai_addr, TIMEOUT_IN_MS);
    assert(!ret);

    freeaddrinfo(addr);

  }

  RDMAClientContext(const char* ipaddr, int port) {
    init(ipaddr, port);
  }

  ~RDMAClientContext() {
    rdma_destroy_event_channel(ec);
  }


private:

  struct rdma_event_channel*  ec;
  struct rdma_cm_id*          conn;
  int                         port;
   

};

class
RDMAServerContext
{
public:

  void init() {

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;

    ec = rdma_create_event_channel();
    assert(ec);

    int ret;
    ret = rdma_create_id(ec, &listener, NULL, RDMA_PS_TCP);
    assert(!ret);

    ret = rdma_bind_addr(listener, (struct sockaddr *)&addr);
    assert(!ret);

    ret = rdma_listen(listener, 10); // 10 = backlog, arbitrary
    assert(!ret);

    port = ntohs(rdma_get_src_port(listener));

    std::cout << "Server: port " << port << std::endl;

  }

  RDMAServerContext() : ec(NULL), listener(NULL), port(0) {
    init();
  }

  ~RDMAServerContext() {
    rdma_destroy_id(listener);
    rdma_destroy_event_channel(ec);
  }


private:

  struct rdma_event_channel*  ec;
  struct rdma_cm_id*          listener;
  int                         port;
  
};

static RDMAServerContext* RDMACreateServerContext()
{
  return new RDMAServerContext();
}


class EioData
{
public:
    EioData();
    ~EioData();
};


Persistent<Function> rdmaConstructor;

static Persistent<String> family_symbol;
static Persistent<String> address_symbol;
static Persistent<String> port_symbol;

class RDMA : public node::ObjectWrap {
public:

  static void Initialize(Handle<Object> target) {

    HandleScope scope;

    Local<FunctionTemplate> t = FunctionTemplate::New(New);
    t->SetClassName(String::NewSymbol("RDMA"));

    t->InstanceTemplate()->SetInternalFieldCount(1);



    // API
    NODE_SET_PROTOTYPE_METHOD(t, "server", Server);
    NODE_SET_PROTOTYPE_METHOD(t, "client", Client);


    rdmaConstructor = Persistent<Function>::New(t->GetFunction());

    family_symbol = NODE_PSYMBOL("family");
    address_symbol = NODE_PSYMBOL("address");
    port_symbol = NODE_PSYMBOL("port");

    target->Set(String::NewSymbol("RDMA"), rdmaConstructor);

  }

  int val;
  RDMAContext ctx;
  RDMAServerContext* serverCtx;
  RDMAClientContext* clientCtx;

private:

  static Handle<Value> Server(const Arguments& args) {
    std::cout << "Server" << std::endl;

    RDMA *rdma = ObjectWrap::Unwrap<RDMA>(args.This());

    rdma->serverCtx = new RDMAServerContext();
    
  }

  static Handle<Value> Client(const Arguments& args) {

    std::cout << "Client" << std::endl;

    // ("addr", port)
    assert(args.Length() >= 2);
    assert(args[0]->IsString());
    assert(args[1]->IsInt32());

    String::AsciiValue ip_address(args[0]->ToString());
    int port = args[1]->Int32Value();

    RDMA *rdma = ObjectWrap::Unwrap<RDMA>(args.This());
    rdma->clientCtx = new RDMAClientContext(*ip_address, port);
    
  }

  static Handle<Value> Bind(const Arguments& args) {
    // ("addr", port)
    assert(args.Length() >= 2);
    assert(args[0]->IsString());
    assert(args[1]->IsInt32());

    RDMA *rdma = ObjectWrap::Unwrap<RDMA>(args.This());

    String::AsciiValue ip_address(args[0]->ToString());
    int port = args[1]->Int32Value();

    std::cout << "Bind" << std::endl;
    std::cout << "addr: " << *ip_address << std::endl;
    std::cout << "port: " << port << std::endl;

  }

  static Handle<Value>  New(const Arguments& args) {

    HandleScope scope;
    if (!args.IsConstructCall()) {
      return args.Callee()->NewInstance();
    }

    try {
      (new RDMA())->Wrap(args.This());
    } catch (const char* msg) {
      return ThrowException(Exception::Error(String::New(msg)));
    }

    return args.This();
  }

  RDMA() {
    val = 3;
  }

  ~RDMA() { }


};

extern "C" {
NODE_MODULE(rdma, RDMA::Initialize);
}
