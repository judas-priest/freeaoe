/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2011  <copyright holder> <email>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once
#include <cstdint>
#include "core/Types.h"

//------------------------------------------------------------------------------
/// This data package contains information about a units status.
//
class UnitStatus
{

public:
    UnitStatus();

    UnitStatus(uint32_t id, uint32_t data_id, MapPos pos);

    virtual ~UnitStatus();

    uint32_t getID();

    uint32_t getDataID();

    MapPos getPos();

private:
    void *player;
    uint32_t id_;
    uint32_t data_id_;

    MapPos pos_;
};

