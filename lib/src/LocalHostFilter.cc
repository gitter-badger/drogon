/**
 *
 *  LocalHostFilter.cc
 *  An Tao
 *  
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include <drogon/LocalHostFilter.h>
#include "HttpResponseImpl.h"
using namespace drogon;
void LocalHostFilter::doFilter(const HttpRequestPtr &req,
                               const FilterCallback &fcb,
                               const FilterChainCallback &fccb)
{
    if (req->peerAddr().toIp() == "127.0.0.1")
    {
        fccb();
        return;
    }
    auto res = drogon::HttpResponse::newNotFoundResponse();
    fcb(res);
}
