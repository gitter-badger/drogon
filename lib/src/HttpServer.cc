/**
 *
 *  HttpServer.cc
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

#include "HttpServer.h"

#include <trantor/utils/Logger.h>
#include "HttpRequestParser.h"
#include "HttpResponseImpl.h"
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/utils/Utilities.h>
#include <functional>

using namespace std::placeholders;
using namespace drogon;
using namespace trantor;

static void defaultHttpAsyncCallback(const HttpRequestPtr &, std::function<void(const HttpResponsePtr &resp)> &&callback)
{
    auto resp = HttpResponse::newNotFoundResponse();
    resp->setCloseConnection(true);
    callback(resp);
}

static void defaultWebSockAsyncCallback(const HttpRequestPtr &,
                                        std::function<void(const HttpResponsePtr &resp)> &&callback,
                                        const WebSocketConnectionPtr &wsConnPtr)
{
    auto resp = HttpResponse::newNotFoundResponse();
    resp->setCloseConnection(true);
    callback(resp);
}

static void defaultConnectionCallback(const trantor::TcpConnectionPtr &conn)
{
    return;
}

HttpServer::HttpServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const std::string &name)
    : _server(loop, listenAddr, name.c_str()),
      _httpAsyncCallback(defaultHttpAsyncCallback),
      _newWebsocketCallback(defaultWebSockAsyncCallback),
      _connectionCallback(defaultConnectionCallback)
{
    _server.setConnectionCallback(
        std::bind(&HttpServer::onConnection, this, _1));
    _server.setRecvMessageCallback(
        std::bind(&HttpServer::onMessage, this, _1, _2));
}

HttpServer::~HttpServer()
{
}

void HttpServer::start()
{
    LOG_TRACE << "HttpServer[" << _server.name()
              << "] starts listenning on " << _server.ipPort();
    _server.start();
}

void HttpServer::onConnection(const TcpConnectionPtr &conn)
{
    if (conn->connected())
    {
        conn->setContext(HttpRequestParser(conn));
        _connectionCallback(conn);
    }
    else if (conn->disconnected())
    {
        LOG_TRACE << "conn disconnected!";
        _connectionCallback(conn);
        HttpRequestParser *requestParser = any_cast<HttpRequestParser>(conn->getMutableContext());
        if (requestParser)
        {
            if (requestParser->webSocketConn())
            {
                _disconnectWebsocketCallback(requestParser->webSocketConn());
            }
#if (CXX_STD > 14)
            conn->getMutableContext()->reset(); //reset(): since c++17
#else
            conn->getMutableContext()->clear();
#endif
        }
    }
}

void HttpServer::onMessage(const TcpConnectionPtr &conn,
                           MsgBuffer *buf)
{
    HttpRequestParser *requestParser = any_cast<HttpRequestParser>(conn->getMutableContext());
    int counter = 0;
    // With the pipelining feature or web socket, it is possible to receice multiple messages at once, so
    // the while loop is necessary 
    while (buf->readableBytes() > 0)
    {
        if (requestParser->webSocketConn())
        {
            //Websocket payload,we shouldn't parse it
            _webSocketMessageCallback(requestParser->webSocketConn(), buf);
            return;
        }
        if (!requestParser->parseRequest(buf))
        {
            conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
            //conn->shutdown();
            requestParser->reset();
            return;
        }
        if (requestParser->gotAll())
        {
            requestParser->requestImpl()->setPeerAddr(conn->peerAddr());
            requestParser->requestImpl()->setLocalAddr(conn->localAddr());
            requestParser->requestImpl()->setReceiveDate(trantor::Date::date());
            if (requestParser->firstReq() && isWebSocket(conn, requestParser->requestImpl()))
            {
                auto wsConn = std::make_shared<WebSocketConnectionImpl>(conn);
                _newWebsocketCallback(requestParser->requestImpl(),
                                      [=](const HttpResponsePtr &resp) mutable {
                                          if (resp->statusCode() == k101SwitchingProtocols)
                                          {
                                              requestParser->setWebsockConnection(wsConn);
                                          }
                                          auto httpString = std::dynamic_pointer_cast<HttpResponseImpl>(resp)->renderToString();
                                          conn->send(httpString);
                                      },
                                      wsConn);
            }
            else
                onRequest(conn, requestParser->requestImpl());
            requestParser->reset();
            counter++;
            if (counter > 1)
                LOG_TRACE << "More than one requests are parsed (" << counter << ")";
        }
        else
        {
            return;
        }
    }
}

bool HttpServer::isWebSocket(const TcpConnectionPtr &conn, const HttpRequestImplPtr &req)
{
    if (req->getHeaderBy("connection") == "Upgrade" &&
        req->getHeaderBy("upgrade") == "websocket")
    {
        LOG_TRACE << "new websocket request";

        return true;
    }
    return false;
}

void HttpServer::onRequest(const TcpConnectionPtr &conn, const HttpRequestImplPtr &req)
{
    const std::string &connection = req->getHeaderBy("connection");
    bool _close = connection == "close" ||
                  (req->getVersion() == HttpRequestImpl::kHttp10 && connection != "Keep-Alive");

    bool isHeadMethod = (req->method() == Head);
    if (isHeadMethod)
    {
        req->setMethod(Get);
    }
    HttpRequestParser *requestParser = any_cast<HttpRequestParser>(conn->getMutableContext());
    requestParser->pushRquestToPipeLine(req);

    _httpAsyncCallback(req, [=](const HttpResponsePtr &response) {
        if (!response)
            return;
        response->setCloseConnection(_close);
        //if the request method is HEAD,remove the body of response(rfc2616-9.4)
        //
        // if (isHeadMethod)
        // {
        //     if (newResp->expiredTime() >= 0)
        //     {
        //         //Cached response,make a copy
        //         newResp = std::make_shared<HttpResponseImpl>(*std::dynamic_pointer_cast<HttpResponseImpl>(response));
        //         newResp->setExpiredTime(-1);
        //     }
        //     newResp->setBody(std::string());
        // }

        auto newResp = response;
        auto &sendfileName = std::dynamic_pointer_cast<HttpResponseImpl>(newResp)->sendfileName();

        if (HttpAppFramework::instance().useGzip() &&
            sendfileName.empty() &&
            req->getHeaderBy("accept-encoding").find("gzip") != std::string::npos &&
            std::dynamic_pointer_cast<HttpResponseImpl>(response)->getHeaderBy("content-encoding") == "" &&
            response->getContentTypeCode() < CT_APPLICATION_OCTET_STREAM &&
            response->getBody().length() > 1024)
        {
            //use gzip
            LOG_TRACE << "Use gzip to compress the body";
            char *zbuf = new char[response->getBody().length()];
            size_t zlen = response->getBody().length();
            if (gzipCompress(response->getBody().data(),
                             response->getBody().length(),
                             zbuf, &zlen) >= 0)
            {
                if (zlen > 0)
                {
                    if (response->expiredTime() >= 0)
                    {
                        //cached response,we need to make a clone
                        newResp = std::make_shared<HttpResponseImpl>(*std::dynamic_pointer_cast<HttpResponseImpl>(response));
                        newResp->setExpiredTime(-1);
                    }
                    newResp->setBody(std::string(zbuf, zlen));
                    newResp->addHeader("Content-Encoding", "gzip");
                }
                else
                {
                    LOG_ERROR << "gzip got 0 length result";
                }
            }
            delete[] zbuf;
        }
        if (conn->getLoop()->isInLoopThread())
        {
            /*
             * A client that supports persistent connections MAY “pipeline”
             * its requests (i.e., send multiple requests without waiting
             * for each response). A server MUST send its responses to those
             * requests in the same order that the requests were received.
             *                                             rfc2616-8.1.1.2
             */
            //std::lock_guard<std::mutex> guard(requestParser->getPipeLineMutex());
            if (conn->disconnected())
                return;
            if (requestParser->getFirstRequest() == req)
            {
                requestParser->popFirstRequest();
                sendResponse(conn, newResp, isHeadMethod);
                while (1)
                {
                    auto resp = requestParser->getFirstResponse();
                    if (resp)
                    {
                        requestParser->popFirstRequest();
                        sendResponse(conn, resp, isHeadMethod);
                    }
                    else
                        return;
                }
            }
            else
            {
                //some earlier requests are waiting for responses;
                requestParser->pushResponseToPipeLine(req, newResp);
            }
        }
        else
        {
            conn->getLoop()->queueInLoop([conn, req, newResp, this, isHeadMethod]() {
                HttpRequestParser *requestParser = any_cast<HttpRequestParser>(conn->getMutableContext());
                if (requestParser)
                {
                    if (requestParser->getFirstRequest() == req)
                    {
                        requestParser->popFirstRequest();
                        sendResponse(conn, newResp, isHeadMethod);
                        while (1)
                        {
                            auto resp = requestParser->getFirstResponse();
                            if (resp)
                            {
                                requestParser->popFirstRequest();
                                sendResponse(conn, resp, isHeadMethod);
                            }
                            else
                                return;
                        }
                    }
                    else
                    {
                        //some earlier requests are waiting for responses;
                        requestParser->pushResponseToPipeLine(req, newResp);
                    }
                }
            });
        }
    });
}

void HttpServer::sendResponse(const TcpConnectionPtr &conn,
                              const HttpResponsePtr &response,
                              bool isHeadMethod)
{
    conn->getLoop()->assertInLoopThread();
    auto respImplPtr = std::dynamic_pointer_cast<HttpResponseImpl>(response);
    if (!isHeadMethod)
    {
        auto httpString = respImplPtr->renderToString();
        conn->send(httpString);
        auto &sendfileName = respImplPtr->sendfileName();
        if (!sendfileName.empty())
        {
            conn->sendFile(sendfileName.c_str());
        }
    }
    else
    {
        auto httpString = respImplPtr->renderHeaderForHeadMethod();
        conn->send(httpString);
    }

    if (response->closeConnection())
    {
        conn->shutdown();
    }
}
