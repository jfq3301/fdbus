/*
 * Copyright (C) 2015   Jeremy Chen jeremy_cz@yahoo.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <common_base/CFdbContext.h>
#include <common_base/CFdbSession.h>
#include <server/CIntraNameProxy.h>
#include <common_base/CLogProducer.h>
#include <utils/Log.h>
#include <iostream>

// template<> FdbSessionId_t CFdbContext::tSessionContainer::mUniqueEntryAllocator = 0;

CFdbContext *CFdbContext::mInstance = 0;

CFdbContext::CFdbContext()
    : CBaseWorker("CFdbContext")
    , mNameProxy(0)
    , mLogger(0)
    , mEnableNameProxy(true)
    , mEnableLogger(true)
{

}

const char *CFdbContext::getFdbLibVersion()
{
    return FDB_DEF_TO_STR(FDB_VERSION_MAJOR) "." FDB_DEF_TO_STR(FDB_VERSION_MINOR) "." FDB_DEF_TO_STR(FDB_VERSION_BUILD);
}

bool CFdbContext::start(uint32_t flag)
{
    LOG_D("[%s][%d]FDB_CONTEXT start=%p.\n", __FUNCTION__, __LINE__, this);
    return CBaseWorker::start(FDB_WORKER_ENABLE_FD_LOOP | flag);
}

bool CFdbContext::init()
{
    return CBaseWorker::init(FDB_WORKER_ENABLE_FD_LOOP);
}

bool CFdbContext::asyncReady()
{
    if (mEnableNameProxy)
    {
        LOG_D("[%s][%d]new CIntraNameProxy.\n", __FUNCTION__, __LINE__);
        auto name_proxy = new CIntraNameProxy();
        name_proxy->connectToNameServer();
        mNameProxy = name_proxy;
    }
    if (mEnableLogger)
    {
        auto logger = new CLogProducer();
        std::string svc_url;
        logger->getDefaultSvcUrl(svc_url);
        LOG_D("[%s][%d]svc_url=%s.\n", __FUNCTION__, __LINE__, svc_url.c_str());
        logger->doConnect(svc_url.c_str());
        mLogger = logger;
    }
    return true;
}

bool CFdbContext::destroy()
{
    if (mNameProxy)
    {
        auto name_proxy = mNameProxy;
        mNameProxy = 0;
        name_proxy->enableNsMonitor(false);
        name_proxy->prepareDestroy();
        delete name_proxy;
    }
    if (mLogger)
    {
        auto logger = mLogger;
        mLogger = 0;
        logger->prepareDestroy();
        delete logger;
    }

    if (!mEndpointContainer.getContainer().empty())
    {
        std::cout << "CFdbContext: Unable to destroy context since there are active endpoint!" << std::endl;
        return false;
    }
    if (!mSessionContainer.getContainer().empty())
    {
        std::cout << "CFdbContext: Unable to destroy context since there are active sessions!\n" << std::endl;
        return false;
    }
    exit();
    join();
    delete this;
    mInstance = 0;
    return true;
}

CBaseEndpoint *CFdbContext::getEndpoint(FdbEndpointId_t endpoint_id)
{
    CBaseEndpoint *endpoint = 0;
    mEndpointContainer.retrieveEntry(endpoint_id, endpoint);
    return endpoint;
}

void CFdbContext::registerSession(CFdbSession *session)
{
    auto sid = mSessionContainer.allocateEntityId();
    session->sid(sid);
    mSessionContainer.insertEntry(sid, session);
}

CFdbSession *CFdbContext::getSession(FdbSessionId_t session_id)
{
    CFdbSession *session = 0;
    mSessionContainer.retrieveEntry(session_id, session);
    return session;
}

void CFdbContext::unregisterSession(FdbSessionId_t session_id)
{
    CFdbSession *session = 0;
    auto it = mSessionContainer.retrieveEntry(session_id, session);
    if (session)
    {
        mSessionContainer.deleteEntry(it);
    }
}

void CFdbContext::deleteSession(FdbSessionId_t session_id)
{
    CFdbSession *session = 0;
    (void)mSessionContainer.retrieveEntry(session_id, session);
    if (session)
    {
        delete session;
    }
}

void CFdbContext::deleteSession(CFdbSessionContainer *container)
{
    auto &session_tbl = mSessionContainer.getContainer();
    for (auto it = session_tbl.begin(); it != session_tbl.end();)
    {
        CFdbSession *session = it->second;
        ++it;
        if (session->container() == container)
        {
            delete session;
        }
    }
}

FdbEndpointId_t CFdbContext::registerEndpoint(CBaseEndpoint *endpoint)
{
    auto id = endpoint->epid();
    if (!fdbValidFdbId(id))
    {
        id = mEndpointContainer.allocateEntityId();
        endpoint->epid(id);
        mEndpointContainer.insertEntry(id, endpoint);
        endpoint->enableMigrate(true);
    }
    return id;
}

void CFdbContext::unregisterEndpoint(CBaseEndpoint *endpoint)
{
    CBaseEndpoint *self = 0;
    auto it = mEndpointContainer.retrieveEntry(endpoint->epid(), self);
    if (self)
    {
        endpoint->enableMigrate(false);
        endpoint->epid(FDB_INVALID_ID);
        mEndpointContainer.deleteEntry(it);
    }
}

void CFdbContext::findEndpoint(const char *name
                               , std::vector<CBaseEndpoint *> &ep_tbl
                               , bool is_server)
{
    auto &container = mEndpointContainer.getContainer();
    for (auto it = container.begin(); it != container.end(); ++it)
    {
        auto endpoint = it->second;
        auto found = false;

        if (is_server)
        {
            if (endpoint->role() == FDB_OBJECT_ROLE_SERVER)
            {
                found = true;
            }
        }
        else if (endpoint->role() == FDB_OBJECT_ROLE_CLIENT)
        {
            found = true;
        }

        if (!found)
        {
            continue;
        }

        if (!endpoint->nsName().compare(name))
        {
            ep_tbl.push_back(endpoint);
        }
    }
}

CIntraNameProxy *CFdbContext::getNameProxy()
{
    return (mNameProxy && mNameProxy->connected()) ? mNameProxy : 0;
}

void CFdbContext::reconnectOnNsConnected()
{
    LOG_D("[%s][%d]enter.\n", __FUNCTION__, __LINE__);
    auto &container = mEndpointContainer.getContainer();
    for (auto it = container.begin(); it != container.end(); ++it)
    {
        auto endpoint = it->second;
        LOG_D("[%s][%d]requestServiceAddress.\n", __FUNCTION__, __LINE__);
        endpoint->requestServiceAddress();
    }
}

void CFdbContext::enableNameProxy(bool enable)
{
    mEnableNameProxy = enable;
}

void CFdbContext::enableLogger(bool enable)
{
    mEnableLogger = enable;
}

CLogProducer *CFdbContext::getLogger()
{
    return mLogger;
}

void CFdbContext::registerNsWatchdogListener(tNsWatchdogListenerFn watchdog_listener)
{
    if (mNameProxy)
    {
        mNameProxy->registerNsWatchdogListener(watchdog_listener);
    }
}

