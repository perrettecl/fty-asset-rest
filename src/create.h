/*  ====================================================================================================================
    create.h - Implementation of POST (create) operation on any asset

    Copyright (C) 2014 - 2020 Eaton

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
    ====================================================================================================================
*/

#pragma once
#include <fty/rest/runner.h>

namespace fty::asset {

class Create: public rest::Runner
{
public:
    INIT_REST("asset/create");

public:
    unsigned run() override;

    Expected<std::string> readName(const std::string& cnt) const;
private:
    // clang-format off
    Permissions m_permissions = {
        { rest::User::Profile::Admin,     rest::Access::Create }
    };
    // clang-format on
};

}
