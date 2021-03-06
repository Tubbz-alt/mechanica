/*
 * MxWindowlessApplication.cpp
 *
 *  Created on: Mar 27, 2019
 *      Author: andy
 */

#include <rendering/MxWindowlessApplication.h>

#include <iostream>

/*


static Magnum::Platform::WindowlessApplication::Configuration
    config(const MxApplication::Configuration) {

    Magnum::Platform::WindowlessApplication::Configuration result;


#ifdef MX_LINUX
    result.clearFlags(Magnum::Platform::WindowlessApplication::Configuration::Flag::ForwardCompatible);
#endif

    return result;
}
*/




MxWindowlessApplication::~MxWindowlessApplication()
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
}

MxWindowlessApplication::MxWindowlessApplication(const Arguments &args) :
    WindowlessApplication{args, Magnum::NoCreate}
{
}

bool MxWindowlessApplication::tryCreateContext(
        const Configuration &conf)
{
    return WindowlessApplication::tryCreateContext(conf);
}

HRESULT MxWindowlessApplication::MxWindowlessApplication::pollEvents()
{
    return E_NOTIMPL;
}

HRESULT MxWindowlessApplication::MxWindowlessApplication::waitEvents()
{
    return E_NOTIMPL;
}

HRESULT MxWindowlessApplication::MxWindowlessApplication::waitEventsTimeout(
        double timeout)
{
    return E_NOTIMPL;
}

HRESULT MxWindowlessApplication::MxWindowlessApplication::postEmptyEvent()
{
    return E_NOTIMPL;
}
