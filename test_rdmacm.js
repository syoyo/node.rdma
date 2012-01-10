var RDMA_CM = require('./build/Release/rdma_cm').RDMA_CM

var rdma_cm = new RDMA_CM()

var e = rdma_cm.create_event_channel()
rdma_cm.destroy_event_channel()

console.log(e)

