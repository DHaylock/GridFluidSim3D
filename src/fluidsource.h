/*
Copyright (c) 2016 Ryan L. Guy

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgement in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/
#ifndef FLUIDSOURCE_H
#define FLUIDSOURCE_H

#include <vector>
#include <stdio.h>
#include <iostream>

#include "vmath.h"
#include "array3d.h"
#include "grid3d.h"
#include "aabb.h"
#include "fluidmaterialgrid.h"
#include "gridindexvector.h"

enum class FluidSourceType : char { 
    inflow   = 0x00, 
    outflow = 0x01
};

class FluidSource
{
public:
    FluidSource();
    FluidSource(vmath::vec3 pos);
    FluidSource(vmath::vec3 pos, vmath::vec3 velocity);
    virtual ~FluidSource();

    void setPosition(vmath::vec3 pos);
    vmath::vec3 getPosition();
    void translate(vmath::vec3 trans);
    void setVelocity(vmath::vec3 v);
    void setDirection(vmath::vec3 dir);
    vmath::vec3 getVelocity();
    void setAsInFlow();
    void setAsOutFlow();
    FluidSourceType getSourceType();
    bool isInflow();
    bool isOutflow();
    void activate();
    void deactivate();
    bool isActive();
    int getID();
    void setID(int identifier);

    virtual GridIndexVector getNewFluidCells(FluidMaterialGrid &materialGrid,
                                                    double dx);
    virtual GridIndexVector getFluidCells(FluidMaterialGrid &materialGrid,
                                                 double dx);
    virtual GridIndexVector getCells(FluidMaterialGrid &materialGrid,
                                            double dx);
    virtual AABB getAABB();

    virtual bool containsPoint(vmath::vec3 p);

protected:

    vmath::vec3 position;
    vmath::vec3 velocity;
    vmath::vec3 direction;

    bool isRunning = true;
    FluidSourceType sourceType = FluidSourceType::inflow;
    int id = 0;

};

#endif
