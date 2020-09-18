/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#ifndef SHARESERVICE_H
#define SHARESERVICE_H

#include <string>
#include <vector>
#include "net.h"
#include "ddb_export.h"

namespace ddb{

class ShareService{
    void handleError(net::Response &res);
public:
    ShareService();
    DDB_DLL void share(const std::vector<std::string> &input, const std::string &tag, const std::string &password, bool recursive);
};

}
#endif // SHARESERVICE_H
