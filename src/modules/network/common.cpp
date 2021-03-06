/**
* This file has been modified from its orginal sources.
*
* Copyright (c) 2012 Software in the Public Interest Inc (SPI)
* Copyright (c) 2012 David Pratt
* Copyright (c) 2012 Mital Vora
* 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
***
* Copyright (c) 2008-2012 Appcelerator Inc.
* 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#ifdef OS_WIN32
#include <tideutils/win/win32_utils.h>
#else
#include <tideutils/posix/posix_utils.h>
#endif
#include <tideutils/file_utils.h>
using namespace TideUtils;

#include "network_module.h"
#include "common.h"

static Logger* GetLogger()
{
    return Logger::Get("Network");
}

void SetCurlProxySettings(CURL* curlHandle, SharedProxy proxy)
{
    if (proxy.isNull())
        return;

    if (proxy->type == SOCKS)
    {
        SET_CURL_OPTION(curlHandle, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5);
    }
    else
    {
        SET_CURL_OPTION(curlHandle, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);
    }

    SET_CURL_OPTION(curlHandle, CURLOPT_PROXY, proxy->host.c_str());
    if (proxy->port != 0)
    {
        SET_CURL_OPTION(curlHandle, CURLOPT_PROXYPORT, proxy->port);
    }

    if (!proxy->username.empty() || !proxy->password.empty())
    {
        // We are allowing any sort of authentication. This may not be the fastest
        // method, but at least the request will succeed.
        std::string proxyAuthString(proxy->username);
        proxyAuthString.append(":");
        proxyAuthString.append(proxy->password.c_str());

        SET_CURL_OPTION(curlHandle, CURLOPT_PROXYUSERPWD, proxyAuthString.c_str());
        SET_CURL_OPTION(curlHandle, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
    }
}

void SetStandardCurlHandleOptions(CURL* handle)
{
    // non negative number means don't verify peer cert - we might want to 
    // make this configurable in the future
    SET_CURL_OPTION(handle, CURLOPT_SSL_VERIFYPEER, 1);
    SET_CURL_OPTION(handle, CURLOPT_CAINFO,
        ::UTF8ToSystem(ti::NetworkModule::GetRootCertPath()).c_str());

    // If a timeout happens, this normally causes cURL to fire a signal.
    // Since we're running multiple threads and possibily have multiple HTTP
    // requests going at once, we need to disable this behavior.
    SET_CURL_OPTION(handle, CURLOPT_NOSIGNAL, 1);

    // Enable all supported Accept-Encoding values.
    SET_CURL_OPTION(handle, CURLOPT_ENCODING, "");

    SET_CURL_OPTION(handle, CURLOPT_FOLLOWLOCATION, 1);
    SET_CURL_OPTION(handle, CURLOPT_AUTOREFERER, 1);

    static std::string cookieJarFilename;
    if (cookieJarFilename.empty())
    {
        cookieJarFilename = FileUtils::Join(
            Host::GetInstance()->GetApplication()->GetDataPath().c_str(),
            "cookies.dat", 0);
    }

    // cURL doesn't have built in thread support, so we must handle thread-safety
    // via the CURLSH callback API.
    SET_CURL_OPTION(handle, CURLOPT_SHARE, ti::NetworkModule::GetCurlShareHandle());
    SET_CURL_OPTION(handle, CURLOPT_COOKIEFILE, cookieJarFilename.c_str());
    SET_CURL_OPTION(handle, CURLOPT_COOKIEJAR, cookieJarFilename.c_str());
}

BytesRef ObjectToBytes(TiObjectRef dataObject)
{
    // If this object is a Bytes object, just do the cast and return.
    BytesRef bytes(dataObject.cast<Bytes>());
    if (!bytes.isNull())
        return bytes;

    // Now try to treat this object like as a file-like object with
    // a .read() method which returns a Bytes. If this fails we'll
    // return NULL. First open file stream then try reading.
    TiMethodRef nativeOpenMethod(dataObject->GetMethod("open", 0));
    if (nativeOpenMethod.isNull())
        return 0;

    ValueRef streamValue(nativeOpenMethod->Call());
    if (streamValue.isNull() || !streamValue->IsObject())
        return 0;

    TiMethodRef nativeReadMethod(streamValue->ToObject()->GetMethod("read", 0));
    if (nativeReadMethod.isNull())
        return 0;

    ValueRef readValue(nativeReadMethod->Call());
    if (!readValue->IsObject())
        return 0;

    // If this cast fails, it will return NULL, as we expect.
    return readValue->ToObject().cast<Bytes>();
}
