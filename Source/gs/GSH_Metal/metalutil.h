#ifndef METALUTIL_H
#define METALUTIL_H

#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

void assign_device(unsigned long long winId, MTL::Device* device);

CA::MetalDrawable* next_drawable(unsigned long long winId);

#endif // METALUTIL_H
