// 
// Copyright 2011 Light Transport Entertainment, Inc.
//
// Licensed under BSD license
//

// node.js
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
    DDMAMessage*                send_msg;

    char*                       rdma_local_region;
    char*                       rdma_remote_region;

    enum {
        SS_INIT,
        SS_MR_SENT,
        SS_RDMA_SENT,
        SS_DONE_SENT
    } send_state;

    enum {
        RS_INIT,
        RS_MR_RECV,
        RS_DONE_RECV
    } recv_state;
} RDMAConnection;

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

static void Connection(struct rdma_cm_id* id)
{
    RDMAConnection* conn;
    struct ibv_qp_init_attr qp_attr;

    BuildRDMAContext(id->verbs);
    BuildQPAttr(ctx, &qp_attr);

    int ret = rdma_create_qp(id, ctx->pd, &qp_attr);
    assert(!ret);

    conn->id = id;
    conn->qp - id->qp;
    
    conn->send_state = SS_INIT;
    conn->recv_state = RS_INIT;

    conn->connected  = 0;

    RDMARegisterMemory(conn);
    
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

//
// Destroy RDMA peer connection.
//
static void DestroyConnection(void *context)
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

static void OnConnect(void* context)
{
    ((RDMAConnection*)context)->connected = 1;
}

static void* PollCQ(void* ctx)
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

static void OnCompletion(struct ibv_wc* wc)
{
    RDAConnection* conn = (RDMAConnection*)(uintptr_t)wc->wr_id;

    if (wc->status != IBV_WC_SUCCESS) {
        // die(expects IBV_WC_SUCCESS)
    }

    if (wc->opcode & IBV_WC_RECV) {
        conn->recv_state++;

        if (conn->recv_msg->type == MSG_MR) {
            memcpy(&conn->peer_mr, &conn->recv_msg->data.mr, sizeof(conn->peer_mr));
            post_receives(conn);

            if (conn->send_state == SS_INIT) {
                send_mr(conn);
            }
        }
    } else {
        con->send_state++;
    }
}

static void SendMessage(RDMAConnection* conn)
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
    sgw.lkey = conn->send_mr->kley;

    // @fixme { Is this required? }
    while (!conn->connected);

    int ret = ibv_post_send(conn->qp, &wr, &bad_wr);
    assert(!ret);

}

static void SendMR(void *context)
{
    RDMAConnection* conn = (RDMAConnection*)context;

    conn->send_msg->type = MSG_MR;
    memcpy(&conn->send_msg->data.mr, conn->rdma_remote_mr, sizeof(struct ibv_mr));

    SendMessage(conn);
}

class EioData
{
public:
    EioData();
    ~EioData();
};

extern "C" void init(Handle<Object> target) {
    // @todo
}
