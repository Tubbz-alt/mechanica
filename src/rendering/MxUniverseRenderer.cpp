/*
    This file is part of Mechanica.

    Based on Magnum example

    Original authors — credit is appreciated but not required:

        2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2019 —
            Vladimír Vondruš <mosra@centrum.cz>
        2019 — Nghia Truong <nghiatruong.vn@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.
 */


#include "MxSimulator.h"

#include <Corrade/Utility/Assert.h>
#include <Corrade/Containers/ArrayView.h>
#include <rendering/MxUniverseRenderer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/Math/Functions.h>
#include <Magnum/Shaders/Generic.h>
#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/Trade/MeshData.h>

#include <Magnum/Animation/Easing.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/PixelFormat.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Image.h>
#include <Magnum/GL/Context.h>
#include <Magnum/GL/Version.h>
#include <Magnum/SceneGraph/Scene.h>
#include <Magnum/SceneGraph/Camera.h>
#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/Math/FunctionsBatch.h>

#include <Magnum/MeshTools/Compile.h>
#include <Magnum/Primitives/Cube.h>
#include <Magnum/Primitives/Icosphere.h>


#include <rendering/WireframeObjects.h>
#include <rendering/NOMStyle.hpp>

#include <assert.h>
#include <iostream>



using namespace Magnum::Math::Literals;

MxUniverseRenderer::MxUniverseRenderer(MxGlfwWindow *win, float particleRadius):
    window{win}
{
    // py init
    //ob_type = &MxUniverseRenderer_Type;
    //ob_refcnt = 1;

    setupCallbacks();

    GL::Renderer::enable(GL::Renderer::Feature::DepthTest);
    
    GL::Renderer::setClearColor(Color3{0.35f});

    /* Loop at 60 Hz max */
    glfwSwapInterval(1);

    Vector3 origin = MxUniverse::origin();
    Vector3 dim = MxUniverse::dim();

    center = (dim + origin) / 2.;

    // TODO: get the max value
    float sideLength = dim[0];

    /* Setup scene objects and camera */

    /* Setup scene objects */
    _scene.reset(new Scene3D{});
    _drawableGroup.reset(new SceneGraph::DrawableGroup3D{});

    /* Set default camera parameters */
    _defaultCamPosition = Vector3(2*sideLength, 2*sideLength, 2 * sideLength);

    _defaultCamTarget   = {0,0,0};


    /* Set up the camera */
    {
        /* Setup the arcball after the camera objects */
        const Vector3 eye = Vector3(2*sideLength, 1.1*sideLength, sideLength);
        const Vector3 center{};
        const Vector3 up = Vector3::zAxis();

        _arcball = new Magnum::Mechanica::ArcBallCamera(*_scene, eye, center, up, 45.0_degf,
            win->windowSize(), win->framebufferSize());
    }


    /* Setup ground grid */

    // grid is ???
    _grid.reset(new WireframeGrid(_scene.get(), _drawableGroup.get()));
    _grid->transform(Matrix4::scaling(Vector3(1.f))  );

    /* Simulation domain box */
    /* Transform the box to cover the region [0, 0, 0] to [3, 3, 1] */
    _drawableBox.reset(new WireframeBox(_scene.get(), _drawableGroup.get()));

    // box is cube of side length 2 centered at origin
    _drawableBox->transform(Matrix4::scaling(Vector3(sideLength / 2)) );
    _drawableBox->setColor(Color3(1, 1, 0));

    setModelViewTransform(Matrix4::translation(-center));
    
    // set up the sphere rendering...
    sphereShader = Shaders::Phong{
        Shaders::Phong::Flag::VertexColor|
        Shaders::Phong::Flag::InstancedTransformation, 1};
    
    sphereInstanceBuffer = GL::Buffer{};
    
    sphereMesh = MeshTools::compile(Primitives::icosphereSolid(2));
    
    sphereMesh.addVertexBufferInstanced(sphereInstanceBuffer, 1, 0,
        Shaders::Phong::TransformationMatrix{},
        Shaders::Phong::NormalMatrix{},
        Shaders::Phong::Color3{});
    
    // setup up lighting properties. TODO: move these to style
    sphereShader.setShininess(2000.0f)
    .setLightPositions({{-20, 40, 20}})
    .setLightColor({0.9, 0.9, 0.9, 1})
    .setShininess(100)
    .setAmbientColor({0.4, 0.4, 0.4, 1})
    .setDiffuseColor({1, 1, 1, 1})
    .setSpecularColor({0.2, 0.2, 0.2, 1});
    
    // we resize instances all the time.
    sphereMesh.setInstanceCount(0);
}

template<typename T>
MxUniverseRenderer& MxUniverseRenderer::draw(T& camera,
        const Vector2i& viewportSize) {


    // the incomprehensible template madness way of doing things.
    //Containers::ArrayView<const float> data(reinterpret_cast<const float*>(&_points[0]), _points.size() * 3);
    //_bufferParticles.setData(data);

    _dirty = false;

    sphereMesh.setInstanceCount(_Engine.s.nr_parts);

    // invalidate / resize the buffer
    sphereInstanceBuffer.setData({NULL, _Engine.s.nr_parts * sizeof(SphereInstanceData)},
                GL::BufferUsage::DynamicDraw);

    // get pointer to data, give me the damned bytes
    SphereInstanceData* pData = (SphereInstanceData*)(void*)sphereInstanceBuffer.map(0,
            _Engine.s.nr_parts * sizeof(SphereInstanceData),
                GL::Buffer::MapFlag::Write|GL::Buffer::MapFlag::InvalidateBuffer);
    
    

    int i = 0;
    for (int cid = 0 ; cid < _Engine.s.nr_cells ; cid++ ) {
        for (int pid = 0 ; pid < _Engine.s.cells[cid].count ; pid++ ) {
            MxParticle *p  = &_Engine.s.cells[cid].parts[pid];
            MxParticleType *type = &_Engine.types[p->typeId];
            Magnum::Vector3 position = {
                (float)(_Engine.s.cells[cid].origin[0] + _Engine.s.cells[cid].parts[pid].x[0]),
                (float)(_Engine.s.cells[cid].origin[1] + _Engine.s.cells[cid].parts[pid].x[1]),
                (float)(_Engine.s.cells[cid].origin[2] + _Engine.s.cells[cid].parts[pid].x[2])
            };
            pData[i].transformationMatrix =
                    Matrix4::translation(position) *
            Matrix4::scaling(Vector3{(float)p->radius});
            pData[i].normalMatrix =
                    pData[i].transformationMatrix.normalMatrix();
            
            NOMStyle *style = p->style ? p->style : type->style;
            pData[i].color = style->color;
            i++;
        }
    }

    sphereInstanceBuffer.unmap();
    
    sphereShader
        .setProjectionMatrix(camera->projectionMatrix())
        .setTransformationMatrix(camera->cameraMatrix() * modelViewMat)
        .setNormalMatrix(camera->viewMatrix().normalMatrix())
        .draw(sphereMesh);

    return *this;
}

MxUniverseRenderer::~MxUniverseRenderer() {
    std::cout << MX_FUNCTION << std::endl;
}

PyTypeObject MxUniverseRenderer_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "UniverseRenderer",
    .tp_basicsize = sizeof(MxUniverseRenderer),
    .tp_itemsize =       0,
    .tp_dealloc =        [](PyObject *obj) -> void {assert( 0 && "should never dealloc stack object MxUniverseRenderer");},
    .tp_print =          0,
    .tp_getattr =        0,
    .tp_setattr =        0,
    .tp_as_async =       0,
    .tp_repr =           0,
    .tp_as_number =      0,
    .tp_as_sequence =    0,
    .tp_as_mapping =     0,
    .tp_hash =           0,
    .tp_call =           0,
    .tp_str =            0,
    .tp_getattro =       0,
    .tp_setattro =       0,
    .tp_as_buffer =      0,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = "Custom objects",
    .tp_traverse =       0,
    .tp_clear =          0,
    .tp_richcompare =    0,
    .tp_weaklistoffset = 0,
    .tp_iter =           0,
    .tp_iternext =       0,
    .tp_methods =        0,
    .tp_members =        0,
    .tp_getset =         0,
    .tp_base =           0,
    .tp_dict =           0,
    .tp_descr_get =      0,
    .tp_descr_set =      0,
    .tp_dictoffset =     0,
    .tp_init =           0,
    .tp_alloc =          0,
    .tp_new =            0,
    .tp_free =           0,
    .tp_is_gc =          0,
    .tp_bases =          0,
    .tp_mro =            0,
    .tp_cache =          0,
    .tp_subclasses =     0,
    .tp_weaklist =       0,
    .tp_del =            0,
    .tp_version_tag =    0,
    .tp_finalize =       0,
};

HRESULT MyUniverseRenderer_Init(PyObject *m)
{
    if (PyType_Ready(&MxUniverseRenderer_Type)) {
        return -1;
    }



    Py_INCREF(&MxUniverseRenderer_Type);
    if (PyModule_AddObject(m, "UniverseRenderer", (PyObject*)&MxUniverseRenderer_Type)) {
        Py_DECREF(&MxUniverseRenderer_Type);
        return -1;
    }
    return 0;
}



void MxUniverseRenderer::onCursorMove(double xpos, double ypos)
{
    /*
    const Vector2i position(xpos, ypos);



    const Vector2 delta = 3.0f*Vector2{position - _prevMousePosition}/Vector2{window->framebufferSize()};
    _prevMousePosition = position;

    if(window->getMouseButtonState(MxGlfwWindow::MouseButtonLeft) == MxGlfwWindow::Press) {
        _objCamera->transformLocal(
            Matrix4::translation(_rotationPoint)*
            Matrix4::rotationX(-0.51_radf*delta.y())*
            Matrix4::rotationY(-0.51_radf*delta.x())*
            Matrix4::translation(-_rotationPoint));
    } else {
        const Vector3 p = unproject(position, _lastDepth);
        _objCamera->translateLocal(_translationPoint - p); /* is Z always 0?
        _translationPoint = p;
    }

*/


}

Vector3 MxUniverseRenderer::unproject(const Vector2i& windowPosition, float depth) const {
    /* We have to take window size, not framebuffer size, since the position is
       in window coordinates and the two can be different on HiDPI systems */
    const Vector2i viewSize = window->windowSize();
    const Vector2i viewPosition = Vector2i{windowPosition.x(), viewSize.y() - windowPosition.y() - 1};
    const Vector3 in{2.0f*Vector2{viewPosition}/Vector2{viewSize} - Vector2{1.0f}, depth*2.0f - 1.0f};

    return in;
}

void MxUniverseRenderer::onCursorEnter(int entered)
{
}



void MxUniverseRenderer::onRedraw()
{
}

void MxUniverseRenderer::onWindowMove(int x, int y)
{
}

void MxUniverseRenderer::onWindowSizeChange(int x, int y)
{
}

void MxUniverseRenderer::onFramebufferSizeChange(int x, int y)
{
}

void MxUniverseRenderer::draw() {
    GL::defaultFramebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);

    /* Call arcball update in every frame. This will do nothing if the camera
           has not been changed. Otherwise, camera transformation will be
           propagated into the camera objects. */
    bool camChanged = _arcball->update();

    //Magnum::Debug{} << _arcball->projectionMatrix();

    /* Draw objects */

    /* Trigger drawable object to update the particles to the GPU */
    setDirty();
    /* Draw particles */
    draw(_arcball, window->framebufferSize());

    /* Draw other objects (ground grid) */
    _arcball->draw(*_drawableGroup);


    if(camChanged) {
        MxSimulator_Redraw();
    }

}

/*
glfwSetFramebufferSizeCallback
#endif
(_window, [](GLFWwindow* const window, const int w, const int h) {
    auto& app = *static_cast<GlfwApplication*>(glfwGetWindowUserPointer(window));
    #ifdef MAGNUM_TARGET_GL
    ViewportEvent e{app.windowSize(), {w, h}, app.dpiScaling()};
    #else
    ViewportEvent e{{w, h}, app.dpiScaling()};
    #endif
    app.viewportEvent(e);
});

*/

void MxUniverseRenderer::viewportEvent(const int w, const int h) {
    /* Resize the main framebuffer */
    GL::defaultFramebuffer.setViewport({{}, window->framebufferSize()});

    /* Recompute the camera's projection matrix */
    //_camera->setViewport(window->framebufferSize());

    //_arcball->reshape(event.windowSize(), event.framebufferSize());
}

void MxUniverseRenderer::onMouseButton(int button, int action, int mods)
{
}

void MxUniverseRenderer::setupCallbacks()
{

}


void MxUniverseRenderer::viewportEvent(Platform::GlfwApplication::ViewportEvent& event) {
    GL::defaultFramebuffer.setViewport({{}, event.framebufferSize()});

    _arcball->reshape(event.windowSize(), event.framebufferSize());

    // TODO: tell the shader
    //_shader.setViewportSize(Vector2{framebufferSize()});
}

void MxUniverseRenderer::keyPressEvent(Platform::GlfwApplication::KeyEvent& event) {
    switch(event.key()) {
        case Platform::GlfwApplication::KeyEvent::Key::L:
            if(_arcball->lagging() > 0.0f) {
                Debug{} << "Lagging disabled";
                _arcball->setLagging(0.0f);
            } else {
                Debug{} << "Lagging enabled";
                _arcball->setLagging(0.85f);
            }
            break;
        case Platform::GlfwApplication::KeyEvent::Key::R:
            _arcball->reset();
            break;

        default: return;
    }

    event.setAccepted();
    window->redraw();
}

void MxUniverseRenderer::mousePressEvent(Platform::GlfwApplication::MouseEvent& event) {
    /* Enable mouse capture so the mouse can drag outside of the window */
    /** @todo replace once https://github.com/mosra/magnum/pull/419 is in */
    //SDL_CaptureMouse(SDL_TRUE);

    _arcball->initTransformation(event.position());

    event.setAccepted();
    window->redraw(); /* camera has changed, redraw! */

}

void MxUniverseRenderer::mouseReleaseEvent(Platform::GlfwApplication::MouseEvent& event) {

}

void MxUniverseRenderer::mouseMoveEvent(Platform::GlfwApplication::MouseMoveEvent& event) {
    if(!event.buttons()) return;

    if(event.modifiers() & Platform::GlfwApplication::MouseMoveEvent::Modifier::Shift) {
        _arcball->translate(event.position());
    }
    else {
        _arcball->rotate(event.position());
    }

    event.setAccepted();
    window->redraw(); /* camera has changed, redraw! */
}

void MxUniverseRenderer::mouseScrollEvent(Platform::GlfwApplication::MouseScrollEvent& event) {
    const Float delta = event.offset().y();
    if(Math::abs(delta) < 1.0e-2f) return;

    _arcball->zoom(delta);

    event.setAccepted();
    window->redraw(); /* camera has changed, redraw! */
}






//void FluidSimApp::mouseScrollEvent(MouseScrollEvent& event) {
//    const Float delta = event.offset().y();
//    if(Math::abs(delta) < 1.0e-2f) {
//        return;
//    }
//
////    if(_imGuiContext.handleMouseScrollEvent(event)) {
////        /* Prevent scrolling the page */
////        event.setAccepted();
////        return;
////    }
//
//    const Float currentDepth = depthAt(event.position());
//    const Float depth = currentDepth == 1.0f ? _lastDepth : currentDepth;
//    const Vector3 p = unproject(event.position(), depth);
//    /* Update the rotation point only if we're not zooming against infinite
//       depth or if the original rotation point is not yet initialized */
//    if(currentDepth != 1.0f || _rotationPoint.isZero()) {
//        _rotationPoint = p;
//        _lastDepth = depth;
//    }
//
//    /* Move towards/backwards the rotation point in cam coords */
//    _objCamera->translateLocal(_rotationPoint * delta * 0.1f);
//}


//void MxUniverseRenderer::mousePressEvent(MouseEvent& event) {
//
//
//    if((event.button() != MouseEvent::Button::Left)
//       && (event.button() != MouseEvent::Button::Right)) {
//        return;
//    }
//
//    /* Update camera */
//    {
//        _prevMousePosition = event.position();
//        const Float currentDepth = depthAt(event.position());
//        const Float depth = currentDepth == 1.0f ? _lastDepth : currentDepth;
//        _translationPoint = unproject(event.position(), depth);
//
//        /* Update the rotation point only if we're not zooming against infinite
//           depth or if the original rotation point is not yet initialized */
//        if(currentDepth != 1.0f || _rotationPoint.isZero()) {
//            _rotationPoint = _translationPoint;
//            _lastDepth = depth;
//        }
//    }
//
//    _mousePressed = true;
//}
