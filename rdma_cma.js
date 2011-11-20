var RDMA_CMA = require('./build/Release/rdma_cma').RDMA_CMA;

// rdma_create_event_channel
// return: event_channel
var create_event_channel = function() {
  channel = RDMA_CMA.rdma_create_event_channel();
}

var destroy_event_channel = function(channel) {
  RDMA_CMA.rdma_destroy_event_channel(channel);
}

var destroy_event_channel = function(channel) {
  RDMA_CMA.rdma_destroy_event_channel(channel);
}
