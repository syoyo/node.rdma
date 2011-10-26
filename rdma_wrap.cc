// 
// Copyright 2011 Light Transport Entertainment, Inc.
//
// Licensed under BSD license
//

// node.js
#include <v8.h>
#include <node.h>

// C++
#include <iostream>

// RDMA CM
#include <rdma/rdma_cma.h>

using namespace v8;

class EioData
{
public:
    EioData();
    ~EioData();
};

extern "C" void init(Handle<Object> target) {
    // @todo
}
