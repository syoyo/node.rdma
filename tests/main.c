//
// Reference:
// 
// - Wriring RDMA application on Linux
//   http://www.digitalvampire.org/rdma-tutorial-2007/notes.pdf
//
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <rdma/rdma_cma.h>

#define RESOLVE_TIMEOUT_MS (1000)

int main(
    int argc,
    char** argv)
{
    struct rdma_event_channel*  cm_channel;
    struct rdma_cm_id*          cm_id;
    struct rdma_cm_event*       event;
    struct rdma_conn_param      conn_param = {};

    struct ibv_pd*              pd;
    struct ibv_comp_channel*    comp_channel;
    struct ibv_cq*              cq;
    struct ibv_cq*              evt_cq;
    struct ibv_mr*              mr;
    struct ibv_qp_init_attr     qp_attr = {};
    struct ibv_sge              sge;
    struct ibv_send_wr          send_wr = {};
    struct ibv_send_wr*         bad_send_wr;
    struct ibv_recv_wr          recv_wr = {};
    struct ibv_recv_wr*         bad_recv_wr;
    struct ibv_wc               wc;

    struct addrinfo*            res;
    struct addrinfo*            t;
    struct addrinfo             hints = {
            .ai_family  = AF_INET;
            .ai_socktype = SOCK_STREAM;
    }

    cm_channel = rdma_create_event_channel();
    if (!cm_channel) {
        return 1;
    }

    err = rdma_create_id(cm_channel, &cm_id, NULL, RDMA_PS_TCP);
    if (err) {
        return 1;
    }

    n = getaddrinfo(argv[1], "20079", &hints, &res);
    if (n < 0) {
        return 1;
    }

    for (t = res; t != NULL; t = t->ai_next) {
        err = rdma_resove_addr(cm_id, NULL, t->ai_addr, RESOLVE_TIMEOUT_MS);
        if (!err) {
            break;
        }
    }

    if (err) {
        return err;
    }

    err = rdma_get_cm_event(cm_channel, &event);
    if (err) {
        return err;
    }

    if (event->event != RDMA_CM_EVENT_ADDR_RESOLVED) {
        return 1;
    }

    rdma_ack_cm_event(event);

    err = rdma_resolve_route(cm_id, RESOLVE_TIMEOUT_MS);
    if (err) {
        return err;
    }

    err = rdma_get_cm_event(cm_channel, &event);
    if (err) {
        return err;
    }

    if (event->event != RDMA_CM_EVENT_ROUTE_RESOLVED) {
        return 1;
    }

    rdma_ack_cm_event(event);

    pd = ibv_alloc_pd(cm_id->verbs);
    if (!pd) {
        return 1;
    }

    comp_channel = ibv_create_comp_channel(cm_id->verbs);
    if (!comp_channel) {
        return 1;
    }

    cq = ibv_create_cq(cm_id->verbs, 2, NULL, comp_channel, 0);
    if (!cq) {
        return 1;
    }

    if (ibv_req_notify_cq(cq, 0)) {
        return 1;
    }

    buf = calloc(2, sizeof(uint32_t));
    if (!buf) {
        return 1;
    }

    mr = ibv_reg_mr(pd, buf, 2 * sizeof(uint32_t), IBV_ACCESS_LOCAL_WRITE);
    if (!mr) {
        return 1;
    }

    qp_attr.cap.max_send_wr     = 2;
    qp_attr.cap.max_send_sge    = 1;
    qp_attr.cap.max_recv_wr     = 1;
    qp_attr.cap.max_recv_sge    = 1;

    qp_attr.send_cq             = cq;
    qp_attr.recv_cq             = cq;

    qp_attr.qp_type             = IBV_QPT_RC;

    err = rdma_create_qp(cm_id, pd, &qp_attr);
    if (err) {
        return err;
    }

    conn_param.initiator_depth = 1;
    conn_param.retry_count = 7;

    err = rdma_connect(cm_id, &conn_param);
    if (err) {
        return err;
    }

    err = rdma_get_cm_event(cm_channel, &event);
    if (err) {
        return err;
    }

    if (event->event != RDMA_CM_EVENT_ESTABLISHED) {
        return 1;
    }

    memcpy(&server_pdata, event->param.conn.private_data, sizeof server_pdata);

    rdma_ack_cm_event(event);

    sge.addr    = (uintptr_t)buf;
    sge_length  = siezeof(uint32_t);
    sge.lkey    = mr->lkey;

    recv_wr.wr_id = 0;
    recv_wr.sg_list = &sge;
    recv_wr.num_sge = 1;

    if (ibv_post_recv(cm_id->qp, &recv_wr, &bad_recv_wr)) {
        return 1;
    }


        
}

