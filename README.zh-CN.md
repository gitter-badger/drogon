![](https://github.com/an-tao/drogon/wiki/images/drogon-white.jpg)

[![Build Status](https://travis-ci.com/an-tao/drogon.svg?branch=master)](https://travis-ci.com/an-tao/drogon)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/45f8a65ca1844788b9109c0044a618f8)](https://app.codacy.com/app/an-tao/drogon?utm_source=github.com&utm_medium=referral&utm_content=an-tao/drogon&utm_campaign=Badge_Grade_Dashboard)
[![Total alerts](https://img.shields.io/lgtm/alerts/g/an-tao/drogon.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/an-tao/drogon/alerts/)
[![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/an-tao/drogon.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/an-tao/drogon/context:cpp)

**Drogon**是一个基于C++14/17的Http应用框架，使用Drogon可以方便的使用C++构建各种类型的Web应用服务端程序。
本版本库是github上[Drogon工程](https://github.com/an-tao/drogon)的镜像库。**Drogon**是作者非常喜欢的美剧《权力的游戏》中的一条龙的名字(汉译作卓耿)，和龙有关但并不是dragon的误写，为了不至于引起不必要的误会这里说明一下。

Drogon的主要应用平台是Linux，也支持Mac OS、FreeBSD，目前还不支持Windows。它的主要特点如下：

* 网络层使用基于epoll(MacOS/FreeBSD下是kqueue)的NIO框架，提供高并发、高性能的网络IO。详细请见[性能测试](https://gitee.com/an-tao/drogon/wikis/性能测试)；
* 全异步编程模式；
* 支持Http1.0/1.1(server端和client端)；
* 基于template实现了简单的反射机制，使主程序框架、控制器(controller)和视图(view)完全解耦；
* 支持cookies和内建的session；
* 支持后端渲染，把控制器生成的数据交给视图生成Html页面，视图由类似JSP的CSP文件描述，通过CSP标签把C++代码嵌入到Html页面，由drogon的命令行工具在编译阶段自动生成C++代码并编译；
* 支持运行期的视图页面动态加载(动态编译和加载so文件)；
* 非常方便灵活的路径(path)到控制器处理函数(handler)的映射方案；
* 支持过滤器(filter)链，方便在控制器之前执行统一的逻辑(如登录验证、Http Method约束验证等)；
* 支持https(基于OpenSSL实现);
* 支持websocket(server端);
* 支持Json格式请求和应答, 对Restful API应用开发非常友好;
* 支持文件下载和上传,支持sendfile系统调用；
* 支持gzip压缩传输；
* 支持pipelining；
* 提供一个轻量的命令行工具drogon_ctl，帮助简化各种类的创建和视图代码的生成过程；
* 基于NIO实现的异步数据库读写，目前支持PostgreSQL和MySQL(MariaDB)数据库；
* 基于线程池实现sqlite3数据库的异步读写，提供与上文数据库相同的接口；
* 方便的轻量级ORM实现，支持常规的对象到数据库的双向映射操作；

### 更多详情请浏览 [wiki](https://gitee.com/an-tao/drogon/wikis/概述)